@echo off
setlocal
pushd "%~dp0"
if not exist "build" mkdir build
cd build

set s=..\src\
set l=..\dep\
set i=/I %l%

set GLAD_SOURCE=%l%glad\src\glad.c

set SOURCE=%s%proj_main.cpp %GLAD_SOURCE%

set INCLUDES=%i%glfw_33_x64\include\ %i%glad\include\ %i%stb\ 

set LIBRARIES=kernel32.lib gdi32.lib shell32.lib msvcrt.lib libcmt.lib user32.lib Comdlg32.lib opengl32.lib %l%glfw_33_x64\lib-vc2019\glfw3.lib %l%glfw_33_x64\lib-vc2019\glfw3dll.lib 

set ARGS=/Zi
rem set ARGS=/O2

cl %ARGS% /nologo /Femain.exe %INCLUDES% %SOURCE% /link %LIBRARIES% /SUBSYSTEM:CONSOLE
if ERRORLEVEL 1 (
	popd
	exit /b 1
)

move main.exe ..

popd
exit /b 0
