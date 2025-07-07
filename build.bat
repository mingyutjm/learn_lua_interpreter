@REM call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

@echo off
mkdir .\build
pushd .\build
cl -FC -Zi ..\src\main.c
main.exe
popd
