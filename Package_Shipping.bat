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

REM --------------------------------------------------------------
REM Adding a new UPROPERTY to a C++ class or editing a Blueprint CDO
REM directly can leave stale cooked data in Saved\Cooked that no
REM longer matches the current class layout, causing a "Bad export
REM index" crash at runtime. Wipe the old cook cache every time so
REM packaging always does a full, fresh cook.
REM --------------------------------------------------------------

echo Cleaning previous cook cache (Saved\Cooked, Saved\StagedBuilds) to avoid stale export tables...
if exist "%PROJECT_DIR%Saved\Cooked" (
    rmdir /s /q "%PROJECT_DIR%Saved\Cooked"
)
if exist "%PROJECT_DIR%Saved\StagedBuilds" (
    rmdir /s /q "%PROJECT_DIR%Saved\StagedBuilds"
)
echo.

echo Starting packaging - this can take several minutes.
echo (Requires Visual Studio Build Tools to compile the C++ source)
echo.

REM --------------------------------------------------------------
REM 아래 두 cvar는 Shipping 전용 크래시 회피용이라 DefaultEngine.ini에
REM 직접 넣지 않고 여기서 -ini: 오버라이드로 패키징 결과물에만 굽는다.
REM (DefaultEngine.ini에 넣으면 에디터 PIE에도 적용돼 부팅이 느려짐.)
REM   - r.TextureStreaming=0            : Shipping 렌더 에셋 스트리밍 매니저 access violation 회피
REM   - s.AsyncLoadingThreadEnabled=False : 비동기 로딩 스레드의 Blueprint CDO 재구성 레이스 크래시 회피
REM --------------------------------------------------------------

call "%ENGINE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
    -project="%PROJECT_FILE%" ^
    -noP4 ^
    -platform=Win64 ^
    -clientconfig=Shipping ^
    -cook -build -stage -pak -iostore -package -archive ^
    -archivedirectory="%ARCHIVE_DIR%" ^
    -nocompileeditor -skipbuildeditor ^
    -CrashReporter ^
    -ini:Engine:[SystemSettings]:r.TextureStreaming=0 ^
    -ini:Engine:[/Script/Engine.StreamingSettings]:s.AsyncLoadingThreadEnabled=False ^
    -utf8output

set "RESULT=%errorlevel%"

REM --------------------------------------------------------------
REM RunUAT.bat can print a harmless trailing error of its own after
REM AutomationTool already finished, which can stomp on errorlevel
REM with an unrelated non-zero code. Trust the actual output file
REM instead of errorlevel alone to decide success/failure.
REM --------------------------------------------------------------

echo.
if exist "%ARCHIVE_DIR%\Windows\Retrieve.exe" (
    echo ============================================
    echo   PACKAGING SUCCEEDED!
    echo   Executable: %ARCHIVE_DIR%\Windows\Retrieve.exe
    echo ============================================
) else (
    echo ============================================
    echo   PACKAGING FAILED  ^(exit code %RESULT%^)
    echo   Check the log above for details.
    echo ============================================
)

echo.
pause
