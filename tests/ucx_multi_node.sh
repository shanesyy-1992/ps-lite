function cleanup() {
    echo "kill all testing process of ps lite for user $USER"
    pkill -9 -u $USER -f test_benchmark_ucx
    sleep 1
}
trap cleanup EXIT
cleanup # cleanup on startup

export DMLC_NUM_WORKER=2
export DMLC_NUM_SERVER=2
export DMLC_PS_ROOT_URI=127.0.0.1 #### ip1
export BYTEPS_ORDERED_HOSTS=127.0.0.1,127.0.0.2 #### ip1, ip2

export DMLC_PS_ROOT_PORT=8188     # scheduler's port (can random choose)
export DMLC_INTERFACE=eth2        # my RDMA interface
export DMLC_ENABLE_RDMA=1

export BYTEPS_ENABLE_IPC=0

export BYTEPS_NODE_ID=1

if [ $# -eq 0 ] # no other args
then
    # launch scheduler
    export BYTEPS_NODE_ID=0
    DMLC_ROLE=scheduler ./test_benchmark_ucx &
fi

# launch server
DMLC_ROLE=server ./test_benchmark_ucx &

# launch worker, with 30MB data per push pull, 10000 rounds, push_then_pull mode
DMLC_ROLE=worker BENCHMARK_NTHREAD=1 ./test_benchmark_ucx 30000000 1000000000 0
