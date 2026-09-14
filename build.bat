@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -property installationPath`) do set VS=%%i
if "!VS!"=="" ( echo no visual studio found & exit /b 1 )
call "!VS!\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /O2 /W3 src\vm\vm.c src\asm\asm.c src\rt\guard.c src\rt\protect.c examples\keycheck.c /Fe:ftm_demo.exe
if errorlevel 1 ( echo build failed & exit /b 1 )
cl /nologo /O2 /W3 src\vm\vm.c src\asm\asm.c src\asm\expr.c src\rt\guard.c src\rt\protect.c examples\liftdemo.c /Fe:ftm_lift.exe
if errorlevel 1 ( echo build failed & exit /b 1 )
cl /nologo /O2 /W3 src\vm\vm.c src\asm\asm.c src\asm\expr.c src\rt\guard.c src\rt\protect.c tools\ftmc.c /Fe:ftmc.exe
if errorlevel 1 ( echo build failed & exit /b 1 )
cl /nologo /O2 /W3 src\vm\vm.c src\asm\asm.c src\rt\guard.c src\rt\protect.c examples\keygate.c /Fe:ftm_gate.exe
if errorlevel 1 ( echo build failed & exit /b 1 )
del *.obj >nul 2>&1
echo ok
