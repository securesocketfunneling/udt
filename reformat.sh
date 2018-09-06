#!/usr/bin/env bash

find  . -type f \( -iname \*.cpp -o -iname \*.h \) | xargs clang-format -i -style=file
