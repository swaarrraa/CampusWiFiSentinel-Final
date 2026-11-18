#!/usr/bin/env bash
# Linux / macOS build for the Campus WiFi Sentinel server.
set -e
cd "$(dirname "$0")"
mkdir -p bin
g++ -std=c++17 -O2 -Wall -pthread \
  backend/main.cpp \
  backend/server/HttpServer.cpp \
  backend/tests/MeasurementEngine.cpp \
  backend/networking/Socket.cpp \
  backend/networking/UdpServer.cpp \
  backend/threading/ThreadPool.cpp \
  backend/database/Database.cpp \
  backend/utils/Logger.cpp \
  -o bin/campus_wifi_server
echo "Build successful: bin/campus_wifi_server"
