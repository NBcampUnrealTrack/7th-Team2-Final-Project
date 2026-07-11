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
REM Shipping 전용 크래시 회피 cvar(r.TextureStreaming=0,
REM s.AsyncLoadingThreadEnabled=False)는 DefaultEngine.ini에 있다.
REM 과거 여기서 -ini: 오버라이드로 넘겼었는데(#261), BuildCookRun의
REM -ini: 인자는 쿠커 프로세스에만 적용되고 pak에 스테이징되는
REM DefaultEngine.ini에는 구워지지 않는다. 그 결과 패키지 런타임에서
REM 텍스처 스트리밍이 다시 켜져 레벨 배치물 텍스처가 저해상도 밉으로
REM 뭉개져 회색으로 보이는 회귀가 있었다. -ini: 오버라이드를 다시
REM 추가하지 말 것. (에디터는 [SystemSettings]를 읽지 않으므로
REM DefaultEngine.ini에 둬도 에디터/PIE 성능에는 영향 없음.)
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
