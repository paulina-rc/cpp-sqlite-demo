@echo off
REM ===========================================================================
REM  compilar.bat — Compila el servidor sin necesidad de "make".
REM  Requiere g++ y gcc en el PATH (por ejemplo MSYS2 UCRT64: C:\msys64\ucrt64\bin)
REM
REM  Uso:  doble clic, o desde la terminal:  compilar.bat
REM ===========================================================================
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul || (
  echo [ERROR] No se encontro g++ en el PATH.
  echo         Instale MSYS2 y agregue C:\msys64\ucrt64\bin al PATH.
  pause & exit /b 1
)

if not exist build mkdir build

REM --- 1. SQLite: solo la primera vez (tarda ~30 s) ---------------------------
if not exist build\sqlite3.o (
  echo [1/3] Compilando SQLite ^(solo la primera vez, puede tardar^)...
  gcc -O2 -DSQLITE_THREADSAFE=1 -c third_party\sqlite\sqlite3.c -o build\sqlite3.o || goto :error
) else (
  echo [1/3] SQLite ya compilado, se omite.
)

REM --- 2. Nuestro codigo C++ --------------------------------------------------
echo [2/3] Compilando codigo C++...
set FLAGS=-std=c++17 -O2 -Wall -D_WIN32_WINNT=0x0A00 -Isrc -Ithird_party\httplib -Ithird_party\json -Ithird_party\sqlite
g++ %FLAGS% -c src\main.cpp     -o build\main.o     || goto :error
g++ %FLAGS% -c src\database.cpp -o build\database.o || goto :error

REM --- 3. Enlazar -------------------------------------------------------------
echo [3/3] Enlazando...
g++ build\main.o build\database.o build\sqlite3.o -o build\servidor.exe -lws2_32 -static || goto :error

echo.
echo Listo: build\servidor.exe
echo Ejecute  iniciar.bat  para arrancar el servidor.
pause
exit /b 0

:error
echo.
echo [ERROR] La compilacion fallo.
pause
exit /b 1
