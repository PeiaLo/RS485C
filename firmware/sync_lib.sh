#!/bin/sh
# 把共用協定庫複製進各 sketch/中繼器資料夾。
#   C++ 韌體(.ino) 需要 protocol.h + protocol.cpp（Arduino/STM32duino 只編 sketch 資料夾內的 .cpp）
#   MicroPython 中繼器 需要 protocol.py（複製到板子檔案系統時與 main.py 同層）
# 這些副本已 gitignore（lib/ 才是單一真相）；clone 後或改過 lib/ 後跑這支。
#   用法： sh firmware/sync_lib.sh
set -e
cd "$(dirname "$0")/.."
for d in firmware/esp32_terminal firmware/esp32_master firmware/uno_terminal firmware/stm32_terminal firmware/nucleo_master nucleo_terminal; do
  cp lib/protocol.h lib/protocol.cpp "$d/"
  echo "synced C++ -> $d"
done
cp lib/protocol.py firmware/repeater_micropython/
echo "synced py  -> firmware/repeater_micropython"
