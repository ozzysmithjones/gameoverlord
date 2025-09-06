
@echo off
setlocal enabledelayedexpansion

REM Set directories
set "SRC_DIR=src"
set "OUT_DIR=out"
set "OUTPUT=gameoverlord.exe"

REM Check if src directory exists
if not exist "%SRC_DIR%" (
    echo Error: Source directory "%SRC_DIR%" not found.
    exit /b 1
)

REM Create output directory if it doesn't exist
if not exist "%OUT_DIR%" (
    mkdir "%OUT_DIR%"
)

REM Find all C files in the src directory
set "C_FILES="
for /r "%SRC_DIR%" %%f in (*.c) do (
    set "C_FILES=!C_FILES! "%%f""
)

REM Check if any C files were found
if "%C_FILES%"=="" (
    echo Error: No C files found in "%SRC_DIR%" directory.
    exit /b 1
)

REM Compile the program
echo Compiling %OUTPUT%...

REM concatenate output path
set "OUTPUT=%OUT_DIR%\%OUTPUT%"

gcc -std=c99 -Wall -Wextra -o %OUTPUT% %C_FILES%

REM Check if compilation was successful
if %ERRORLEVEL% neq 0 (
    echo Build failed.
    exit /b %ERRORLEVEL%
) else (
    echo Build successful: %OUTPUT% created.
)

endlocal
