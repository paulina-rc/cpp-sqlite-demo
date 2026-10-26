@echo off
REM ===========================================================================
REM  iniciar.bat — Arranca el servidor y abre el navegador.
REM  Se ejecuta desde la carpeta del proyecto para que encuentre public\ y data\
REM ===========================================================================
cd /d "%~dp0"

if not exist build\servidor.exe (
  echo No existe build\servidor.exe. Ejecute primero compilar.bat
  pause & exit /b 1
)

start "" http://localhost:8080
build\servidor.exe 8080
pause
