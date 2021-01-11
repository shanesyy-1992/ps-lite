export BINARY=./test_benchmark_cuda
export BINARY=./test_benchmark_stress
# export BINARY=./test_benchmark_ucx
export ARGS="4096000 999999999 1"
export ARGS="30000000 1000000000 0"
# 30000000 999999999 1"

function cleanup() {
    echo "kill all testing process of ps lite for user $USER"
    if [[ $EUID -ne 0 ]]; then
        pkill -9 -u $USER -f $BINARY
    else
        pkill -9 -f $BINARY
    fi
    sleep 1
}
trap cleanup EXIT
# cleanup # cleanup on startup

export DMLC_NUM_WORKER=${DMLC_NUM_WORKER:-1}
export DMLC_NUM_SERVER=$DMLC_NUM_WORKER
export CUDA_HOME=/opt/tiger/cuda_10_0
export LD_LIBRARY_PATH=$CUDA_HOME/lib64:/data00/home/haibin.lin/ps-lite-repos/ps-lite-test-benchmark-gcc4-prefix-librdma-master/ucx_build/lib

# export NODE_ONE_IP=10.0.0.1 # sched and server
# export NODE_TWO_IP=10.0.0.2 # worker

export DMLC_PS_ROOT_URI=${NODE_ONE_IP}  # try eth2
export BYTEPS_ORDERED_HOSTS=${NODE_ONE_IP},${NODE_TWO_IP}

export DMLC_PS_ROOT_PORT=${DMLC_PS_ROOT_PORT:-12279} # scheduler's port (can random choose)
export UCX_IB_TRAFFIC_CLASS=236
export DMLC_INTERFACE=eth2        # my RDMA interface
export UCX_TLS=ib,cuda
export DMLC_ENABLE_RDMA=1
export DMLC_ENABLE_UCX=${DMLC_ENABLE_UCX:-1}          # enable ucx
# export PS_VERBOSE=2

# export UCX_MEMTYPE_CACHE=n
# export UCX_RNDV_SCHEME=put_zcopy
# export BYTEPS_UCX_SHORT_THRESH=0

export LOCAL_SIZE=0               # test ucx gdr
export CUDA_VISIBLE_DEVICES=0,1
# export UCX_IB_GPU_DIRECT_RDMA=yes

export BYTEPS_ENABLE_IPC=0

if [ $# -eq 0 ] # no other args
then
    # launch scheduler
    echo "This is scheduler node."
    export BYTEPS_NODE_ID=0
    export DMLC_NODE_HOST=${NODE_ONE_IP}
    export UCX_RDMA_CM_SOURCE_ADDRESS=${NODE_ONE_IP}

    DMLC_ROLE=scheduler $BINARY &
    if [ $DMLC_NUM_WORKER == "2" ]; then
      DMLC_ROLE=worker BENCHMARK_NTHREAD=1 $BINARY $ARGS &
    fi
    # launch server
    DMLC_ROLE=server $BINARY
fi

export DMLC_NODE_HOST=${NODE_TWO_IP}
export UCX_RDMA_CM_SOURCE_ADDRESS=${NODE_TWO_IP}
export BYTEPS_NODE_ID=1

if [ $DMLC_NUM_WORKER == "2" ]; then
  DMLC_ROLE=server BENCHMARK_NTHREAD=1 $BINARY &
fi
DMLC_ROLE=worker BENCHMARK_NTHREAD=1 $BINARY $ARGS
