@echo off
rem Создаёт минимальный самодостаточный пакет ядра Stuff
rem (blib-system + blib-core + тесты) в папке deploy рядом со скриптом.
rem В пакет копируются все coreonly-скрипты сборки как есть —
rem сборку получатель запускает сам той конфигурацией, что нужна:
rem   cmake_build_debug_coreonly.bat   / cmake_build_debug_coreonly.sh
rem   cmake_build_release_coreonly.bat / cmake_build_release_coreonly.sh

setlocal
set "SCRIPT_DIR=%~dp0"
set "DEPLOY_DIR=%SCRIPT_DIR%deploy"

echo [1/2] Cleaning deploy folder...
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"

echo [2/2] Copying sources and build scripts...
mkdir "%DEPLOY_DIR%\src\blib"
copy /Y "%SCRIPT_DIR%src\CMakeLists.txt" "%DEPLOY_DIR%\src\" >nul
copy /Y "%SCRIPT_DIR%src\blib\CMakeLists.txt" "%DEPLOY_DIR%\src\blib\" >nul
copy /Y "%SCRIPT_DIR%src\blib\align.h" "%DEPLOY_DIR%\src\blib\" >nul
copy /Y "%SCRIPT_DIR%src\blib\blibint.h" "%DEPLOY_DIR%\src\blib\" >nul
copy /Y "%SCRIPT_DIR%src\blib\config.h" "%DEPLOY_DIR%\src\blib\" >nul
copy /Y "%SCRIPT_DIR%src\blib\inline.h" "%DEPLOY_DIR%\src\blib\" >nul
copy /Y "%SCRIPT_DIR%src\blib\utilmacro.h" "%DEPLOY_DIR%\src\blib\" >nul
xcopy "%SCRIPT_DIR%src\blib\system" "%DEPLOY_DIR%\src\blib\system\" /E /I /Y >nul
xcopy "%SCRIPT_DIR%src\blib\core" "%DEPLOY_DIR%\src\blib\core\" /E /I /Y >nul
xcopy "%SCRIPT_DIR%src\blib\test" "%DEPLOY_DIR%\src\blib\test\" /E /I /Y >nul
copy /Y "%SCRIPT_DIR%cmake_build_debug_coreonly.bat" "%DEPLOY_DIR%\" >nul
copy /Y "%SCRIPT_DIR%cmake_build_release_coreonly.bat" "%DEPLOY_DIR%\" >nul
copy /Y "%SCRIPT_DIR%cmake_build_debug_coreonly.sh" "%DEPLOY_DIR%\" >nul
copy /Y "%SCRIPT_DIR%cmake_build_release_coreonly.sh" "%DEPLOY_DIR%\" >nul

echo Deploy completed successfully. Package: %DEPLOY_DIR%
endlocal
