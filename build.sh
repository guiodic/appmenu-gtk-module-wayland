#!/bin/bash
export CFLAGS="${CFLAGS/-fno-plt/}"
export LDFLAGS="${LDFLAGS/-Wl,-z,now/}"
cmake -B build .
cmake --build build
