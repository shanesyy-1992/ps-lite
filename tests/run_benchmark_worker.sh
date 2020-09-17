source tests/port_util.sh
export DMLC_NUM_WORKER=1
export DMLC_NUM_SERVER=1 
export DMLC_PS_ROOT_URI=10.130.23.14 # 10.0.0.2  # scheduler's RDMA interface IP 
export DMLC_INTERFACE=eth2        # my RDMA interface 
DMLC_ROLE=worker ./stress_test_benchmark 30000000 10000 3
