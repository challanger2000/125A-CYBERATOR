@echo off
setlocal
set "MODE=%~1"
if not defined MODE set "MODE=normal"
set "PSModulePath=%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-test.ps1" -Mode "%MODE%"
set "RESULT=%ERRORLEVEL%"
echo.
echo Test exit code: %RESULT% ^(0=pass, 3=deadline misses, other=execution failure^)
if not defined CI pause
exit /b %RESULT%
