@echo off
setlocal
pushd "%~dp0"
if not exist "build" mkdir build
cd build

set s=..\src\
set l=..\dep\
set i=/I %l%


set GLAD_SOURCE=%l%glad\src\glad.c
set SOURCE=%s%proj_main.cpp %s%proj_sound.cpp %s%proj_math.cpp %GLAD_SOURCE%
set INCLUDES=%i%glfw_33_x64\include\ %i%glad\include\ %i%stb\ 

set LIBRARIES=kernel32.lib gdi32.lib shell32.lib msvcrt.lib libcmt.lib user32.lib Comdlg32.lib ole32.lib opengl32.lib %l%glfw_33_x64\lib-vc2019\glfw3.lib %l%glfw_33_x64\lib-vc2019\glfw3dll.lib 


if "%1" == "-t"     goto TESTING
if "%1" == "--test" goto TESTING

if "%1" == "-d"      goto DEBUGGING
if "%1" == "--debug" goto DEBUGGING

echo [Release Enabled]
echo.
set ARGS=/O2
set LINK_ARGS=/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
goto COMPILE

:TESTING
    echo [Tests Enabled]
    echo.
    set ARGS=/Zi /DTESTING_ENABLE
    set LINK_ARGS=/SUBSYSTEM:CONSOLE
    goto COMPILE
:DEBUGGING
    echo [Debug Enabled]
    echo.
    set ARGS=/Zi /DDEBUG_ENABLE
    set LINK_ARGS=/SUBSYSTEM:CONSOLE
    goto COMPILE

:COMPILE
set NAME=sudoku
cl %ARGS% /nologo /F 2000000 /Fe%NAME%.exe %INCLUDES% %SOURCE% /link %LIBRARIES% %LINK_ARGS%
if ERRORLEVEL 1 (
	popd
	exit /b 1
)

REM REM -- Add Icon
REM rcedit-x64.exe %NAME%.exe --set-icon ..\res\icon.ico
REM 
REM REM -- Make Archive
REM tar -cf %NAME%.zip %NAME%.exe
REM mv %NAME%.zip ..
REM cd ..
REM tar -rf %NAME%.zip res
REM tar -rf %NAME%.zip shaders
REM mv %NAME%.zip build/%NAME%.zip
REM cd build

move %NAME%.exe ..

popd
exit /b 0
