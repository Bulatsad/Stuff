#!/bin/sh
# Сборка ядра Stuff (Debug): blib-system, blib-core и их тесты,
# затем прогон тестов.
# Платформенные модули (graphics/sound/network) отключаются на
# конфигурации, поэтому beng не конфигурируется. Кэш linuxbuild
# общий с полной сборкой. Этот скрипт также используется как
# скрипт сборки deploy-пакета ядра (см. deploy_coreonly.sh).
# Папка linuxbuild рядом с src. Скрипт можно запускать из любого каталога.

set -e

cd "$(dirname "$0")"

echo "[1/3] Configuring (Debug)..."
cmake -S src -B linuxbuild -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON \
    -DBLIB_BUILD_GRAPHICS=OFF -DBLIB_BUILD_SOUND=OFF -DBLIB_BUILD_NETWORK=OFF

echo "[2/3] Building (core + tests: blib-system, blib-core, blib_tests)..."
cmake --build linuxbuild --config Debug --target blib-system blib-core blib_tests

echo "[3/3] Running tests..."
ctest --test-dir linuxbuild --output-on-failure

echo "Build completed successfully."
