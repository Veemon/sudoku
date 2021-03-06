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

echo.

:ICON
WHERE rcedit-x64.exe >nul 2>nul
IF %ERRORLEVEL% NEQ 0  goto ARCHIVE
echo [Linking Icon]
rcedit-x64.exe %NAME%.exe --set-icon ..\res\icon2.ico

:ARCHIVE
if "%1" == "-d"      goto CONCLUDE
if "%1" == "--debug" goto CONCLUDE
echo [Packaging]
tar -cf %NAME%.zip %NAME%.exe
mv %NAME%.zip ..
cd ..
tar -rf %NAME%.zip res
tar -rf %NAME%.zip shaders
tar -rf %NAME%.zip README.md
mv %NAME%.zip build/%NAME%.zip
cd build

:CONCLUDE
move %NAME%.exe ..

popd
exit /b 0
