@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
where gcc >nul 2>nul
if errorlevel 1 (
  echo Khong tim thay gcc. Hay cai MinGW va them vao PATH.
  exit /b 1
)
if not exist build\sqlite3.o (
  echo Dang bien dich SQLite...
  gcc -c -O2 -w -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -Ithird_party\sqlite-amalgamation-3460100 third_party\sqlite-amalgamation-3460100\sqlite3.c -o build\sqlite3.o
  if errorlevel 1 exit /b 1
)
gcc -std=c11 -O2 -Wall -Wno-unused-function -Wno-unused-variable -D_WIN32_WINNT=0x0601 -finput-charset=UTF-8 -fexec-charset=UTF-8 -Ic -Ithird_party\sqlite-amalgamation-3460100 ^
  c\json.c c\sha256.c c\util.c c\db.c c\http.c c\auth.c c\profile.c c\upgrade.c c\catalog.c c\question.c c\exam.c c\attempt.c c\dash.c c\ops.c c\seed.c c\routes.c c\main.c ^
  build\sqlite3.o -o build\online_exam.exe -lws2_32
if errorlevel 1 exit /b 1
echo.
echo Da bien dich: build\online_exam.exe
echo Chay bang run.bat
