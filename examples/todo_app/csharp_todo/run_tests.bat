@echo off
setlocal
if exist "%ProgramFiles%\dotnet\dotnet.exe" (
  set "PATH=%ProgramFiles%\dotnet;%PATH%"
)
if exist "%ProgramFiles%\dotnet\x64\dotnet.exe" (
  set "PATH=%ProgramFiles%\dotnet\x64;%PATH%"
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_tests.ps1" %*
exit /b %ERRORLEVEL%
