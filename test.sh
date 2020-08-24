export DMLC_NUM_WORKER=1
export DMLC_NUM_SERVER=1
# export DMLC_PS_ROOT_URI=10.130.22.212  # scheduler's RDMA interface IP
export DMLC_PS_ROOT_URI=10.130.23.14  # try eth2
export DMLC_PS_ROOT_PORT=8123     # scheduler's port (can random choose)
export DMLC_INTERFACE=eth0        # my RDMA interface
export DMLC_ENABLE_RDMA=1

# launch scheduler
DMLC_ROLE=scheduler ./tests/test_benchmark &

# launch server
DMLC_ROLE=server ./tests/test_benchmark
