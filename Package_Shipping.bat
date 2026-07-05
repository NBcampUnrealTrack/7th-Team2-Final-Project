@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   Retrieve - Windows Shipping Packager
echo ============================================
echo.

REM --------------------------------------------------------------
REM Place this script in the project root (next to Retrieve.uproject)
REM and just double-click it to run.
REM --------------------------------------------------------------

set "PROJECT_DIR=%~dp0"
set "PROJECT_FILE=%PROJECT_DIR%Retrieve.uproject"
set "ARCHIVE_DIR=%PROJECT_DIR%..\Package"

if not exist "%PROJECT_FILE%" (
    echo [ERROR] Retrieve.uproject was not found next to this script.
    echo Keep this .bat file in the project root folder and try again.
    echo Current folder: %PROJECT_DIR%
    pause
    exit /b 1
)

REM --------------------------------------------------------------
REM Auto-detect the Unreal Engine 5.7 install location.
REM If your engine is installed somewhere else, edit ENGINE_ROOT below.
REM   Example: set "ENGINE_ROOT=D:\UE_5.7"
REM --------------------------------------------------------------

set "ENGINE_ROOT="

for %%P in (
    "C:\Program Files\Epic Games\UE_5.7"
    "D:\Program Files\Epic Games\UE_5.7"
    "C:\Epic Games\UE_5.7"
    "D:\Epic Games\UE_5.7"
) do (
    if not defined ENGINE_ROOT (
        if exist "%%~P\Engine\Build\BatchFiles\RunUAT.bat" (
            set "ENGINE_ROOT=%%~P"
        )
    )
)

if not defined ENGINE_ROOT (
    echo [ERROR] Could not auto-detect an Unreal Engine 5.7 install.
    echo Open this file in a text editor, uncomment the line below,
    echo set it to your actual engine path, and run again:
    echo.
    echo   REM set "ENGINE_ROOT=C:\Program Files\Epic Games\UE_5.7"
    echo.
    pause
    exit /b 1
)

echo Engine  : %ENGINE_ROOT%
echo Project : %PROJECT_FILE%
echo Output  : %ARCHIVE_DIR%\Windows\Retrieve.exe
echo.
echo Starting packaging - this can take several minutes.
echo (Requires Visual Studio Build Tools to compile the C++ source)
echo.

call "%ENGINE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
    -project="%PROJECT_FILE%" ^
    -noP4 ^
    -platform=Win64 ^
    -clientconfig=Shipping ^
    -cook -build -stage -pak -iostore -package -archive ^
    -archivedirectory="%ARCHIVE_DIR%" ^
    -nocompileeditor -skipbuildeditor ^
    -CrashReporter ^
    -utf8output

set "RESULT=%errorlevel%"

echo.
if not "%RESULT%"=="0" (
    echo ============================================
    echo   PACKAGING FAILED  ^(exit code %RESULT%^)
    echo   Check the log above for details.
    echo ============================================
) else (
    echo ============================================
    echo   PACKAGING SUCCEEDED!
    echo   Executable: %ARCHIVE_DIR%\Windows\Retrieve.exe
    echo ============================================
)

echo.
pause
