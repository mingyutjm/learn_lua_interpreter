@REM call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

@echo off
mkdir .\build
pushd .\build
cl /utf-8 /FC /Zi /Fe:main.exe ..\src\main.c ..\src\test\test1.c ..\src\test\test2.c ..\src\test\test3.c
@REM cl /utf-8 /FC /Zi /Fe:main.exe ..\src\main.c ..\src\luaaux.c ..\src\luado.c ..\src\luamem.c ..\src\luastate.c ..\src\luagc.c
echo =================================
main.exe
popd