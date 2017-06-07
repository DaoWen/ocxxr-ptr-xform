#-------------------------------------------------------------------------------
# Shows how to build the samples vs. a released binary download of LLVM & Clang.
#
# Assumes the binary release was downloaded from http://llvm.org/releases/
# and untarred in some directory. The actual script code here uses a location
# I use on my machine, but that can be replaced by anything you fancy in the
# BINARY_DIR_PATH variable.
#
# Eli Bendersky (eliben@gmail.com)
# This code is in the public domain
#-------------------------------------------------------------------------------

#!/bin/bash

set -eu
set -x

#BINARY_DIR_PATH=$HOME/llvm/llvm3.8-binaries
BINARY_DIR_PATH=/usr/lib/llvm-3.8

make -j4 \
  CXX=$BINARY_DIR_PATH/bin/clang++ \
  LLVM_SRC_PATH=$BINARY_DIR_PATH \
  LLVM_BUILD_PATH=$BINARY_DIR_PATH/bin \
  LLVM_BIN_PATH=$BINARY_DIR_PATH/bin

# default to test target
[ -z "$*" ] && unshift test

make \
  CXX=$BINARY_DIR_PATH/bin/clang++ \
  LLVM_SRC_PATH=$BINARY_DIR_PATH \
  LLVM_BUILD_PATH=$BINARY_DIR_PATH/bin \
  LLVM_BIN_PATH=$BINARY_DIR_PATH/bin \
  "$@"
