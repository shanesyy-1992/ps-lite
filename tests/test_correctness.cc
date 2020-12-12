#include <chrono>
#include <cmath>
#include <thread>
#include <cstdlib>
#include <unistd.h>
#include <cuda_runtime.h>
#include "ps/ps.h"

#define DIVUP(x, y) (((x)+(y)-1)/(y))
#define ROUNDUP(x, y) (DIVUP((x), (y))*(y))
#define DEBUG_PRINT_TENSOR_VALUE(X) (*((float *)(X) + 0))
#define DEBUG_PRINT_TENSOR_ADDRESS(X) (reinterpret_cast<uint64_t>(X))

#define CUDA_CALL(func)                                      \
  {                                                          \
    cudaError_t e = (func);                                  \
    CHECK(e == cudaSuccess || e == cudaErrorCudartUnloading) \
        << "CUDA: " << cudaGetErrorString(e);                \
  }

using namespace ps;

std::unordered_map<uint64_t, KVPairs<char> > mem_map;
bool debug_mode_ = false;

// gpu_idx runs from -1 (cpu) to MAX_GPU_ID
void aligned_memory_alloc(void** ptr, size_t size, int gpu_idx) {
  // We set value to be equal to its gpu_idx
  if (gpu_idx == -1) {
    // CPU Alloc
    size_t page_size = sysconf(_SC_PAGESIZE);
    void* p;
    int size_aligned = ROUNDUP(size, page_size);
    int ret = posix_memalign(&p, page_size, size_aligned);
    CHECK_EQ(ret, 0) << "posix_memalign error: " << strerror(ret);
    CHECK(p);
    memset(p, gpu_idx, size);
    *ptr = p;
  } else {
    // GPU Alloc, malloc should automatically gives page aligned.
    CUDA_CALL(cudaSetDevice(gpu_idx));
    CUDA_CALL(cudaMalloc(ptr, size));
    CUDA_CALL(cudaMemset(ptr, gpu_idx, size));
  }
}

void set_val(void* ptr, size_t size, int gpu_idx, int val) {
  if (gpu_idx == -1) {
    // CPU set
    memset(ptr, val, size);
  } else {
    // GPU set
    CUDA_CALL(cudaSetDevice(gpu_idx));
    CUDA_CALL(cudaMemset(ptr, val, size));
  }
}

void check_val(void* ptr, size_t size, int gpu_idx, int val) {
  char* p;
  if (gpu_idx == -1) {
    // CPU check
    p = (char *) ptr;
  } else {
    // GPU check
    CUDA_CALL(cudaMemcpy((void *) p, ptr, size, cudaMemcpyDeviceToHost));
  }

  for (int i = 0; i < size; ++ i) {
    CHECK_EQ(p[i], (char) val);
  }
}

void float_sum(float *dst, float *src, size_t len) {
  if (len == 0) return;
  for (size_t i = 0; i < len / (size_t) sizeof(float); ++i) {
    dst[i] = dst[i] + src[i];
  }
}

template <typename Val>
void EmptyHandler(const KVMeta &req_meta, const KVPairs<Val> &req_data, KVServer<Val> *server) {
  uint64_t key = req_data.keys[0];
  if (req_meta.push) {
    CHECK(req_data.lens.size());
    CHECK_EQ(req_data.vals.size(), (size_t)req_data.lens[0]) 
        << "key=" << key << ", " << req_data.vals.size() << ", " << req_data.lens[0];

    if (mem_map.find(key) == mem_map.end()) {
      size_t len = (size_t) req_data.vals.size();

      void* ptr_val;
      aligned_memory_alloc(&ptr_val, len, -1);  
      mem_map[key].vals.reset((char*)ptr_val, len, [](void *){ });

      void* ptr_key;
      aligned_memory_alloc(&ptr_key, sizeof(Key), -1);  
      mem_map[key].keys.reset((Key*)ptr_key, 1, [](void *){ });
      memcpy(ptr_key, &key, sizeof(Key));

      void* ptr_len;
      aligned_memory_alloc(&ptr_len, sizeof(int), -1);  
      mem_map[key].lens.reset((int*)ptr_len, 1, [](void *){ });
      memcpy(ptr_len, &len, sizeof(int));
    }

    auto recved = reinterpret_cast<char*>(req_data.vals.data());
    // only sum the first 4 bytes
    size_t sum_len = debug_mode_ ? req_data.vals.size() : 0;
    float_sum((float*) mem_map[key].vals.data(), (float*) recved, sum_len);

    if (debug_mode_) {
      LOG(INFO) << "recved tensor! key=" << key << "\t"
          << "store: " << DEBUG_PRINT_TENSOR_VALUE(mem_map[key].vals.data()) << "\t"
          << "recv: " << DEBUG_PRINT_TENSOR_VALUE(recved) << "\t"
          << "address: " << DEBUG_PRINT_TENSOR_ADDRESS(recved) << "\t"
          << "len: " << req_data.vals.size() << "\t"
          << "sender: " << req_meta.sender;
    }

    // send push response (empty)
    KVPairs<char> res;
    server->Response(req_meta, res);
  }
  else {
    auto iter = mem_map.find(key);
    CHECK_NE(iter, mem_map.end());
    server->Response(req_meta, iter->second);
  }
}

void StartServer() {
  if (!IsServer()) return;
  debug_mode_ = Environment::Get()->find("DEBUG_MODE") ? true : false;

  auto server = new KVServer<char>(0);
  server->set_request_handle(EmptyHandler<char>);
  RegisterExitCallback([server]() { delete server; });
}

void RunWorker(int argc, char *argv[], KVWorker<char>* kv, int tid) {
  auto krs = ps::Postoffice::Get()->GetServerKeyRanges();

  const int num_servers = krs.size();
  LOG(INFO) << num_servers << " servers in total";
  CHECK_GT(num_servers, 0);

  // init
  int len = (argc > 1) ? atoi(argv[1]) : 1024000;
  // int repeat = (argc > 2) ? atoi(argv[2]) : 10;

  auto v = Environment::Get()->find("NUM_KEY_PER_SERVER");
  const int how_many_key_per_server = v ? atoi(v) : 10;
  const int total_key_num = num_servers * how_many_key_per_server;

  std::vector<SArray<char> > server_vals;
  std::vector<SArray<Key> > server_keys;
  std::vector<SArray<int> > server_lens;
  std::vector<int> gpu_indices;
  std::vector<void*> gpu_ptrs;

  // Round robin alloc each val in different GPUs, cpu_id = -1
  auto local_size_str = Environment::Get()->find("LOCAL_SIZE");
  auto local_size = local_size_str ? atoi(local_size_str) : 0;
  LOG(INFO) << "GPU LOCAL SIZE (num of gpu) " << local_size;

  for (int key = 0; key < total_key_num; key++) {
    void* ptr;
    if (local_size == 0) {
      // Normal all cpu unit test
      LOG(INFO) << "Allocating val on CPU with size " << len;
      aligned_memory_alloc(&ptr, len, - 1 /* gpu_idx */);
      gpu_indices.push_back(-1);
    } else {
      int idx = key % (local_size + 1) - 1;
      if (idx != -1) {
        LOG(INFO) << "Allocating val on GPU " << idx << " with size " << len;
      } else {
        LOG(INFO) << "Allocating val on CPU " << " with size " << len;
      }
      aligned_memory_alloc(&ptr, len, idx /* gpu_idx */);
      gpu_indices.push_back(idx);
    }
    gpu_ptrs.push_back(ptr);
    SArray<char> vals;
    vals.reset((char*) ptr, len * sizeof(char), [](void *){});
    server_vals.push_back(vals);
    key ++;
  }

  // init push, do not count this into time cost
  for (int key = 0; key < total_key_num; key++) {
    int server = key % num_servers;
    PS_VLOG(1) << "key=" << key << " assigned to server " << server;

    auto vals = server_vals[key];

    // page aligned keys
    void* ptr_key;
    aligned_memory_alloc(&ptr_key, sizeof(Key), -1);
    SArray<Key> keys;
    keys.reset((Key*) ptr_key, 1, [](void *){});
    ps::Key ps_key = krs[server].begin() + key;
    memcpy(ptr_key, &ps_key, sizeof(Key));
    server_keys.push_back(keys);

    // page aligned vals
    void* ptr_len;
    aligned_memory_alloc(&ptr_len, sizeof(int), -1);
    SArray<int> lens;
    lens.reset((int*) ptr_len, 1, [](void *){});
    memcpy(ptr_len, &len, sizeof(len));
    server_lens.push_back(lens);
    kv->Wait(kv->ZPush(keys, vals, lens));
  }

  int repeat = 3;
  for (int i = 1; i < repeat; ++ i){
    for (int key_idx = 0; key_idx < total_key_num; key_idx++) {
      auto keys = server_keys[key_idx];
      auto lens = server_lens[key_idx];
      auto vals = server_vals[key_idx];
      auto gpu_idx = gpu_indices[key_idx];
      auto ptr = gpu_ptrs[key_idx];

      kv->Wait(kv->ZPush(keys, vals, lens));
      // Set the raw data to absolutely wrong ones and pull
      // the correct value should be gpu_idx.
      set_val(ptr, len, gpu_idx, -2);
      kv->Wait(kv->ZPull(keys, &vals, &lens));
      check_val(ptr, len, gpu_idx, gpu_idx);
    }
  }
}

int main(int argc, char *argv[]) {
  // disable multi-threaded processing first
  setenv("ENABLE_SERVER_MULTIPULL", "0", 1);

  auto v = Environment::Get()->find("BENCHMARK_NTHREAD");
  const int nthread = v ? atoi(v) : 1;
  LOG(INFO) << "number of threads for the same worker = " << nthread;

  // start system
  Start(0);
  // setup server nodes
  StartServer();
  // run worker nodes
  if (IsWorker()) {
    KVWorker<char> kv(0, 0);
    std::vector<std::thread> threads;
    for (int i = 0; i < nthread; ++i) {
      threads.emplace_back(RunWorker, argc, argv, &kv, threads.size());
    }
    for (int i = 0; i < nthread; ++i) {
      threads[i].join();
      LOG(INFO) << "Thread " << i << " is done.";
    }
  }
  // stop system
  Finalize(0, true);
  return 0;
}
