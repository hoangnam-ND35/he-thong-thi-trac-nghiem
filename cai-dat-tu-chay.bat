@echo off
REM Cai dat: server tu bat khi dang nhap Windows, khong can run.bat.
cd /d "%~dp0"

if not exist build\online_exam.exe (
  call build.bat
  if errorlevel 1 (
    echo Bien dich that bai.
    pause
    exit /b 1
  )
)

set "STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup"
set "LINK=%STARTUP%\Phong thi truc tuyen.lnk"
set "VBS=%~dp0start-server.vbs"
set "OPEN=%~dp0mo-web.vbs"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut($env:LINK); $s.TargetPath = 'wscript.exe'; $s.Arguments = '\"' + $env:VBS + '\"'; $s.WorkingDirectory = '%~dp0'; $s.WindowStyle = 7; $s.Description = 'Phong thi truc tuyen - chay nen'; $s.Save(); Write-Host 'Da cai vao Startup'"

REM Tao shortcut mo web tren Desktop
set "DESK=%USERPROFILE%\Desktop"
set "DESKLINK=%DESK%\Mo Phong thi.lnk"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut($env:DESKLINK); $s.TargetPath = 'wscript.exe'; $s.Arguments = '\"' + $env:OPEN + '\"'; $s.WorkingDirectory = '%~dp0'; $s.Description = 'Mo Phong thi tren Chrome'; $s.Save(); Write-Host 'Da tao shortcut Desktop'"

REM Bat server ngay
wscript.exe "%~dp0start-server.vbs"
timeout /t 2 /nobreak >nul
start "" "http://127.0.0.1:8080/"

echo.
echo Xong.
echo - Server chay nen, khong can run.bat
echo - Mo may tinh se tu bat server
echo - Bam shortcut "Mo Phong thi" tren Desktop de mo Chrome
echo.
echo Luu y: GitHub Pages khong chay duoc server C++. Web nay chay tren may ban: http://127.0.0.1:8080
pause
