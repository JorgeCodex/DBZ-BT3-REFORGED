@echo off
setlocal
cd /d "%~dp0.."
where cl.exe >nul 2>&1
if errorlevel 1 (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
  if errorlevel 1 (
    echo No se encontro el compilador MSVC. Instala Visual Studio Build Tools con C++ y vuelve a intentarlo.
    exit /b 1
  )
)
if not exist build mkdir build
pushd build
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT /DRADITZ_FLIGHT /LD "..\src\adventure\intro_video.cpp" "..\src\adventure\flight.cpp" "..\src\adventure\adventure_loader.cpp" /link /OUT:DBZBT3R.dll /IMPLIB:DBZBT3R.lib mfplay.lib ole32.lib user32.lib
set "result=%errorlevel%"
popd
if not "%result%"=="0" exit /b %result%
if exist "external\local-only\DBZBT3R\" (
  copy /y "build\DBZBT3R.dll" "external\local-only\DBZBT3R\DBZBT3R.dll" >nul
  if errorlevel 1 (
    echo No se pudo actualizar la DLL del paquete local.
    exit /b 1
  )
  echo DLL actualizada en external\local-only\DBZBT3R\
)
echo Compilacion terminada: build\DBZBT3R.dll

