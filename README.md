Follow the steps below to reproduce the test result:

## Code Prepare

ps-lite code: use current branch

ucx code: https://github.com/openucx/ucx/tree/7125ef17e69a54716076954fa286f3628959c9e0

## Install Dependencies
```
apt update
apt install -y build-essential libtool autoconf automake libnuma-dev unzip pkg-config
apt install -y libibverbs-dev librdmacm-dev ibverbs-providers
```

## Build UCX

Get into UCX directory and 

```
./autogen.sh || ./autogen.sh && ./contrib/configure-release --enable-profiling --enable-frame-pointer --enable-debug-data --enable-memtrack --enable-logging --enable-mt --prefix=${UCX_HOME} && make clean && make && sudo make install;
```

## Build ps-lite

Get into ps-lite directory and 
```
rm -rf deps
make clean
export USE_UCX=1
export USE_RDMA=1
make VERBOSE=1 ADD_CFLAGS="-I${UCX_HOME}/include -L${UCX_HOME}/lib"
```

## Run ps-lite test

Get into ps-lite directory and
```
export NODE_ONE_IP={YOUR_SERVER_IP}
export NODE_TWO_IP={YOUR_WORKER_IP}
export LD_LIBRARY_PATH=${UCX_HOME}/lib:$LD_LIBRARY_PATH

cd tests

# launch server:
DMLC_NUM_PORTS=1 SKIP_DEV_ID_CHECK=1 LOCAL_SIZE=0 taskset -c 0-31 bash ./ucx_multi_node_old.sh

# launch worker:
DMLC_NUM_PORTS=1 SKIP_DEV_ID_CHECK=1 LOCAL_SIZE=0 TEST_ENABLE_CPU=1 taskset -c 0-31 ./ucx_multi_node_old.sh {TEST_MSG_SIZE}
```
