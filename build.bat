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


if "%1" == "-t"     goto TESTING
if "%1" == "--test" goto TESTING

if "%1" == "-d"      goto DEBUGGING
if "%1" == "--debug" goto DEBUGGING

set ARGS=/O2
goto COMPILE

:TESTING
    echo [Tests Enabled]
    echo.
    set ARGS=/Zi /DTESTING_ENABLE
    goto COMPILE
:DEBUGGING
    echo [Debug Enabled]
    echo.
    set ARGS=/Zi /DDEBUG_ENABLE
    goto COMPILE

:COMPILE
set EXE_NAME=sudoku.exe
cl %ARGS% /nologo /Fe%EXE_NAME% %INCLUDES% %SOURCE% /link %LIBRARIES% /SUBSYSTEM:CONSOLE
if ERRORLEVEL 1 (
	popd
	exit /b 1
)

rcedit-x64.exe %EXE_NAME% --set-icon ..\res\icon.ico
move %EXE_NAME% ..

popd
exit /b 0
