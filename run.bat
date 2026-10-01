@echo off
cd /d "%~dp0"
if not exist build\online_exam.exe (
  call build.bat
  if errorlevel 1 exit /b 1
)
build\online_exam.exe %*
