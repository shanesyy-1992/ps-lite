function cleanup() {
    echo "kill all testing process of ps lite for user $USER"
    pkill -9 -u $USER -f stress_test_benchmark
    sleep 1
}
trap cleanup EXIT
cleanup # cleanup on startup

export DMLC_NUM_WORKER=1
export DMLC_NUM_SERVER=1
export DMLC_PS_ROOT_URI=10.130.23.14  # try eth2
export BYTEPS_ORDERED_HOSTS=10.130.23.14

export DMLC_PS_ROOT_PORT=8188     # scheduler's port (can random choose)
export DMLC_INTERFACE=eth2        # my RDMA interface
export DMLC_ENABLE_RDMA=1

export BYTEPS_ENABLE_IPC=0

export BYTEPS_NODE_ID=0
DMLC_ROLE=scheduler ./stress_test_benchmark &

# # # launch server
# DMLC_ROLE=server ./stress_test_benchmark &

# launch worker, with 30MB data per push pull, 10000 rounds, push_then_pull mode
# DMLC_ROLE=worker BENCHMARK_NTHREAD=1 gdb7.12 -ex run --args ./stress_test_benchmark 30000000 1000000000 0

export PS_VERBOSE=2
export BYTEPS_PRINT_RDMA_LOG=1

if [[ $1 = "-g" ]]; then
    DMLC_ROLE=worker BENCHMARK_NTHREAD=1 gdb7.12 -ex run --args ./stress_test_benchmark 300000000 1000000000 0
else
    DMLC_ROLE=worker BENCHMARK_NTHREAD=1 ./stress_test_benchmark 300000000 1000000000 0
fi
