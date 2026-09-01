@echo off
rem RS485_C 同步協定庫到各 sketch / 中繼器資料夾（Windows 雙擊即可執行）
rem 等同 firmware/sync_lib.sh。改過 lib/ 或 clone 後跑一次。
cd /d "%~dp0.."
for %%d in (esp32_terminal esp32_master uno_terminal stm32_terminal nucleo_master) do (
  copy /Y "lib\protocol.h" "firmware\%%d\" >nul
  copy /Y "lib\protocol.cpp" "firmware\%%d\" >nul
  echo synced C++  -^> firmware\%%d
)
copy /Y "lib\protocol.py" "firmware\repeater_micropython\" >nul
echo synced py   -^> firmware\repeater_micropython
echo.
echo Done.
pause
