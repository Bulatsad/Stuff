# ============================================================
# Запуск всех тестов blib
#
# Находит собранные blib_test_*.exe и прогоняет каждый в своём
# процессе (изоляция: крэш одной группы не валит остальные).
#
#   .\run_tests.ps1                 - только запуск
#   .\run_tests.ps1 -Build          - сборка + запуск
#   .\run_tests.ps1 -Config Release - другой конфиг
# ============================================================

param(
	[string]$Config = "Debug",
	[string]$BuildDir = "..\..\..\build",
	[switch]$Build
)

$ErrorActionPreference = "Stop"

# Сборка по требованию
if ($Build)
{
	Write-Host "== Building tests ($Config) =="
	cmake --build $BuildDir --config $Config
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Расположение exe зависит от генератора:
# VS (multi-config) -> <build>/blib/test/<Config>/, Ninja -> <build>/blib/test/
$testDirCandidates = @("$BuildDir\blib\test\$Config", "$BuildDir\blib\test")
$exeDir = $testDirCandidates | Where-Object { Test-Path "$_\blib_test_angle.exe" } | Select-Object -First 1

if (-not $exeDir)
{
	Write-Error "Не найдены собранные тесты (blib_test_*.exe). Соберите: cmake -B $BuildDir -DBUILD_TESTS=ON"
	exit 1
}

$exes = Get-ChildItem "$exeDir\blib_test_*.exe" | Sort-Object Name

$passed = 0
$failed = 0

foreach ($exe in $exes)
{
	Write-Host ""
	Write-Host "== $($exe.Name) =="
	& $exe.FullName
	if ($LASTEXITCODE -eq 0) { $passed++ } else { $failed++ }
}

Write-Host ""
Write-Host "== Итого: $passed прошло, $failed упало =="
if ($failed -gt 0) { exit 1 }
exit 0
