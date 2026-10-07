@echo off
REM Mo cong 8080 tren Windows Firewall de dien thoai/may khac vao duoc.
REM Can chay bang quyen Administrator.

net session >nul 2>&1
if errorlevel 1 (
  echo Hay chuot phai file nay ^> Run as administrator
  pause
  exit /b 1
)

netsh advfirewall firewall delete rule name="Phong thi truc tuyen 8080" >nul 2>&1
netsh advfirewall firewall add rule name="Phong thi truc tuyen 8080" dir=in action=allow protocol=TCP localport=8080
echo Da mo cong 8080.
echo Dien thoai cung Wi-Fi mo: http://192.168.1.4:8080
echo ^(Neu IP may ban khac, xem trong trang Giao bai^)
pause
