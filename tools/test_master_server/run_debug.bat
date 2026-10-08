@echo off
setlocal

cd /d "%~dp0"

dotnet build TestMasterServer.csproj -c Debug -nologo -v quiet
if errorlevel 1 (
    pause
    exit /b 1
)

bin\Debug\net9.0\TestMasterServer.exe %*
set EXIT_CODE=%ERRORLEVEL%

if not "%EXIT_CODE%"=="0" pause
exit /b %EXIT_CODE%
