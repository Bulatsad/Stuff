#!/bin/sh
# Полная сборка проекта Stuff (Release) и прогон тестов.
# Конфигурация и сборка выполняются в папке linuxbuild рядом с src
# (рядом со скриптом). Скрипт можно запускать из любого каталога.

set -e

cd "$(dirname "$0")"

echo "[1/3] Configuring (Release)..."
cmake -S src -B linuxbuild -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

echo "[2/3] Building..."
cmake --build linuxbuild --config Release

echo "[3/3] Running tests..."
ctest --test-dir linuxbuild --output-on-failure

echo "Build completed successfully."
