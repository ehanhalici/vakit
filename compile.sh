#!/bin/bash

bear -- make

# sadece cc de
# make CC="clang -MJ compile_commands.json"

# ninja
# ninja -t compdb cc cxx > compile_commands.json

# cmake
# cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
