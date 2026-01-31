#!/bin/bash

xgettext \
  --language=C++ \
  --from-code=UTF-8 \
  --keyword=_ \
  --keyword=N_ \
  -o po/fcitx5-vmk.pot \
  $(find . \( -name "*.cpp" -o -name "*.h" -o -name "*.in" \))
