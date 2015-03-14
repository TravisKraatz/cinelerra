#!/bin/bash

for d in guicast/ cinelerra/ plugins/*; do
  if [ ! -d "$d" ]; then continue; fi
  ls -1 $d/*.[Ch] $d/*.inc 2> /dev/null;
done | xgettext --no-wrap -L C++ -k_ -kN_ -f - -o -

