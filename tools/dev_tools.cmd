@echo off
setlocal
if defined NEXTCLIENT_PYTHON goto configured_python
set "PANEL_PYTHON=%USERPROFILE%\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
if exist "%PANEL_PYTHON%" goto bundled_python
where py >nul 2>nul
if not errorlevel 1 goto python_launcher
python -X utf8 "%~dp0dev_tools\dev_tools.py" %*
exit /b %errorlevel%
:configured_python
"%NEXTCLIENT_PYTHON%" -X utf8 "%~dp0dev_tools\dev_tools.py" %*
exit /b %errorlevel%
:bundled_python
"%PANEL_PYTHON%" -X utf8 "%~dp0dev_tools\dev_tools.py" %*
exit /b %errorlevel%
:python_launcher
py -3 -X utf8 "%~dp0dev_tools\dev_tools.py" %*
exit /b %errorlevel%
