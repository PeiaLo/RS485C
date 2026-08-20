#!/bin/sh
# 把共用協定庫 lib/protocol.* 複製進各 ESP32/UNO sketch 資料夾。
# Arduino IDE 只編 sketch 資料夾內的 .cpp，故每個 sketch 需要一份 lib 副本。
# 這些副本已被 gitignore（lib/ 才是單一真相）；clone 後、或改過 lib/ 後，跑這支重新同步。
#   用法： sh firmware/sync_lib.sh
set -e
cd "$(dirname "$0")/.."
for d in firmware/esp32_terminal firmware/esp32_master firmware/uno_terminal; do
  cp lib/protocol.h lib/protocol.cpp "$d/"
  echo "synced -> $d"
done
