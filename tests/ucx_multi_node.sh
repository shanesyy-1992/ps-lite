function cleanup() {
    echo "kill all testing process of ps lite for user $USER"
    if [[ $EUID -ne 0 ]]; then
        pkill -9 -u $USER -f stress_test_benchmark
        pkill -9 -u $USER -f test_benchmark
    else
        pkill -9 -f stress_test_benchmark
        pkill -9 -f test_benchmark
    fi
    sleep 1
}
trap cleanup EXIT
cleanup # cleanup on startup

export DMLC_NUM_WORKER=2
export DMLC_NUM_SERVER=2

export NODE_ONE_IP=127.0.0.1
export NODE_TWO_IP=127.0.0.2

export DMLC_PS_ROOT_URI=${NODE_ONE_IP}  # try eth2
export BYTEPS_ORDERED_HOSTS=${NODE_ONE_IP},${NODE_TWO_IP}
export DMLC_NODE_HOST=${NODE_TWO_IP}  # by default it's remote

export DMLC_PS_ROOT_PORT=9194     # scheduler's port (can random choose)
export DMLC_INTERFACE=eth2        # my RDMA interface
# export DMLC_ENABLE_RDMA=1
export DMLC_ENABLE_UCX=1          # test ucx
export UCX_TLS=all                # not working
# export UCX_TLS=ib,tcp           # working

export LOCAL_SIZE=4               # test ucx gdr
export CUDA_VISIBLE_DEVICES=0,1,2,3
export UCX_IB_GPU_DIRECT_RDMA=no

export BYTEPS_ENABLE_IPC=0

export BYTEPS_NODE_ID=1

if [ $# -eq 0 ] # no other args
then
    # launch scheduler
    echo "This is scheduler node."
    export BYTEPS_NODE_ID=0
    export DMLC_NODE_HOST=${NODE_ONE_IP}
    DMLC_ROLE=scheduler ./test_benchmark &
fi

# launch server
DMLC_ROLE=server ./test_benchmark &

# launch worker, with 30MB data per push pull, 10000 rounds, push_then_pull mode
DMLC_ROLE=worker BENCHMARK_NTHREAD=1 ./test_benchmark 30000000 10240 1

# for correctness test, use this following line and replace previous
# scheduler / server binary with ./test_correctness

# DMLC_ROLE=worker BENCHMARK_NTHREAD=1 ./test_correctness 30000000
