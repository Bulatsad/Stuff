@echo off
rem Сборка ядра Stuff (Release): blib-system, blib-core и их тесты,
rem затем прогон тестов.
rem Платформенные модули (graphics/sound/network) отключаются на
rem конфигурации, поэтому beng не конфигурируется. Кэш winbuild
rem общий с полной сборкой. Этот скрипт также используется как
rem скрипт сборки deploy-пакета ядра (см. deploy_coreonly.bat).
rem Папка winbuild рядом с src. Скрипт можно запускать из любого каталога.

setlocal
set "SCRIPT_DIR=%~dp0"

echo [1/3] Configuring (Release)...
cmake -S "%SCRIPT_DIR%src" -B "%SCRIPT_DIR%winbuild" -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_TESTS=ON ^
    -DBLIB_BUILD_GRAPHICS=OFF -DBLIB_BUILD_SOUND=OFF -DBLIB_BUILD_NETWORK=OFF
if errorlevel 1 (
    echo ERROR: cmake configuration failed
    exit /b 1
)

echo [2/3] Building (core + tests: blib-system, blib-core, blib_tests)...
cmake --build "%SCRIPT_DIR%winbuild" --config Release --target blib-system blib-core blib_tests
if errorlevel 1 (
    echo ERROR: build failed
    exit /b 1
)

echo [3/3] Running tests...
ctest --test-dir "%SCRIPT_DIR%winbuild" -C Release --output-on-failure
if errorlevel 1 (
    echo ERROR: tests failed
    exit /b 1
)

echo Build completed successfully.
endlocal
