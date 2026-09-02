@echo off
rem Полная сборка проекта Stuff (Release) и прогон тестов.
rem Конфигурация и сборка выполняются в папке winbuild рядом с src
rem (рядом со скриптом). Скрипт можно запускать из любого каталога.
rem Папка winbuild общая для Debug и Release (multi-config).
rem Платформенные модули и тесты включаются явно: кэш общий
rem со coreonly-сборками, которые отключают платформенные модули.

setlocal
set "SCRIPT_DIR=%~dp0"

echo [1/3] Configuring (Release)...
cmake -S "%SCRIPT_DIR%src" -B "%SCRIPT_DIR%winbuild" -DCMAKE_BUILD_TYPE=Release -Dblib_link_type=blib_link_static ^
    -DBUILD_TESTS=ON ^
    -DBLIB_BUILD_GRAPHICS=ON -DBLIB_BUILD_SOUND=ON -DBLIB_BUILD_NETWORK=ON
if errorlevel 1 (
    echo ERROR: cmake configuration failed
    exit /b 1
)

echo [2/3] Building...
cmake --build "%SCRIPT_DIR%winbuild" --config Release
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
