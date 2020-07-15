cc_library(
    name="ps",
    srcs=["src/*.cc"],
    incs=["src",
          "include"],
    export_incs=["include"],
    deps=["cpp3rdlib/zmq:4.1.4@//cpp3rdlib/zmq:zmq",
          "cpp3rdlib/glog:0.3.3@//cpp3rdlib/glog:glog"
          ],
    defs=["DMLC_USE_RDMA"],
    optimize=["-O3", "-g", "-fopenmp", "-Wall", "-Wextra", "-std=c++11"],
    extra_linkflags=["-ldl"],
)
