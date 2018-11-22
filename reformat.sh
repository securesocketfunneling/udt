#!/usr/bin/env bash

find  apps include test -type f \( -iname \*.cpp -o -iname \*.h \) | xargs clang-format -i -style=file
