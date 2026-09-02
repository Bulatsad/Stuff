#!/bin/sh
# Создаёт минимальный самодостаточный пакет ядра Stuff
# (blib-system + blib-core + тесты) в папке deploy рядом со скриптом.
# В пакет копируются все coreonly-скрипты сборки как есть —
# сборку получатель запускает сам той конфигурацией, что нужна:
#   cmake_build_debug_coreonly.bat   / cmake_build_debug_coreonly.sh
#   cmake_build_release_coreonly.bat / cmake_build_release_coreonly.sh

set -e

cd "$(dirname "$0")"

DEPLOY_DIR="$PWD/deploy"

echo "[1/2] Cleaning deploy folder..."
rm -rf "$DEPLOY_DIR"

echo "[2/2] Copying sources and build scripts..."
mkdir -p "$DEPLOY_DIR/src/blib"
cp src/CMakeLists.txt "$DEPLOY_DIR/src/"
cp src/blib/CMakeLists.txt "$DEPLOY_DIR/src/blib/"
cp src/blib/align.h src/blib/blibint.h src/blib/config.h src/blib/inline.h src/blib/utilmacro.h "$DEPLOY_DIR/src/blib/"
cp -r src/blib/system "$DEPLOY_DIR/src/blib/"
cp -r src/blib/core "$DEPLOY_DIR/src/blib/"
cp -r src/blib/test "$DEPLOY_DIR/src/blib/"
cp cmake_build_debug_coreonly.bat cmake_build_release_coreonly.bat \
   cmake_build_debug_coreonly.sh cmake_build_release_coreonly.sh "$DEPLOY_DIR/"

echo "Deploy completed successfully. Package: $DEPLOY_DIR"
