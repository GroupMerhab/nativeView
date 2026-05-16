@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_static.ps1" %*
exit /b %ERRORLEVEL%
