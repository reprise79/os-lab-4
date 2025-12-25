@echo off
git pull origin main

if not exist build mkdir build
cd build

cmake -G "MinGW Makefiles" ..
cmake --build .

start "simulator" simulator.exe COM5
start "logger" logger.exe COM6
pause