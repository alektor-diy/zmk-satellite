#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
${CC:-cc} -std=c11 -Wall -Wextra -Werror -Isrc \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  tests/test_protocol.c src/protocol.c src/state.c -o build-host/test_protocol
./build-host/test_protocol
