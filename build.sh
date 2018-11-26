#!/bin/bash

root_dir=$(cd $(dirname $0) && pwd)
cd $root_dir

# Run protoc to generate C++ code.
#
mkdir -p build/generated
( cd src && ${root_dir}/third_party/install/protobuf-3.6.1/bin/protoc --cpp_out=${root_dir}/build/generated ryu.proto )

# Compile ryu binary.
#
mkdir -p build/bin
g++ -std=c++14 -O3 -o build/bin/ryu \
    -I./third_party/install/protobuf-3.6.1/include \
    -I./src \
    -I./build/generated \
    src/ryu.cc \
    build/generated/ryu.pb.cc \
    third_party/install/protobuf-3.6.1/lib/libprotobuf.a

# Create the default preamble.
#
cat src/default_preamble.ryu | \
    sed -e "s,%PROTOBUF_INSTALL%,$root_dir/third_party/install/protobuf-3.6.1,g" \
    >build/bin/ryu.preamble
