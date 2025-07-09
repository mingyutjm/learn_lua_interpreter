@REM call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

@echo off
mkdir .\build
pushd .\build
cl /utf-8 /FC /Zi /Fe:main.exe ..\src\main.c
@REM cl /utf-8 /FC /Zi /Fe:main.exe ..\src\main.c ..\src\luaaux.c ..\src\luado.c ..\src\luamem.c ..\src\luastate.c
echo =================================
@REM main.exe
popd