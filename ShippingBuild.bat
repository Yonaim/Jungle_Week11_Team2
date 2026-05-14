@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SOLUTION_DIR=%~dp0"
set "PROJECT_DIR=%SOLUTION_DIR%KraftonEngine"
set "SOLUTION_FILE=%SOLUTION_DIR%KraftonEngine.sln"
set "CONFIGURATION=Shipping"
set "PLATFORM=x64"
set "BUILD_OUTPUT=%PROJECT_DIR%\Bin\%CONFIGURATION%"
set "STAGE_DIR=%SOLUTION_DIR%ShippingBuild"
set "COOKED_SCENE_DIR=%SOLUTION_DIR%Saved\Cooked\%CONFIGURATION%\Asset\Content\Scene"
set "EXE_NAME=KraftonEngine.exe"
set "NO_PAUSE=0"
if /I "%~1"=="-NoPause" set "NO_PAUSE=1"

echo ============================================
echo  KraftonEngine BuildCookRun
echo  Configuration: %CONFIGURATION% %PLATFORM%
echo ============================================

call :FindVisualStudio || goto :Failed

echo.
echo [1/5] Build
msbuild "%SOLUTION_FILE%" /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM% /m /v:minimal
if errorlevel 1 (
    echo ERROR: MSBuild failed.
    goto :Failed
)

if not exist "%BUILD_OUTPUT%\%EXE_NAME%" (
    echo ERROR: Build output was not found: "%BUILD_OUTPUT%\%EXE_NAME%"
    goto :Failed
)

echo.
echo [2/5] Cook
if exist "%COOKED_SCENE_DIR%" (
    rmdir /s /q "%COOKED_SCENE_DIR%"
    if exist "%COOKED_SCENE_DIR%" (
        echo ERROR: Could not clean "%COOKED_SCENE_DIR%".
        goto :Failed
    )
)
mkdir "%COOKED_SCENE_DIR%" || goto :Failed

pushd "%PROJECT_DIR%" >nul
"%BUILD_OUTPUT%\%EXE_NAME%" -cook -cookoutput="%COOKED_SCENE_DIR%"
set "COOK_RESULT=%ERRORLEVEL%"
popd >nul
if not "%COOK_RESULT%"=="0" (
    echo ERROR: Cook failed.
    goto :Failed
)

echo.
echo [3/5] Prepare staging directory
if exist "%STAGE_DIR%" (
    rmdir /s /q "%STAGE_DIR%"
    if exist "%STAGE_DIR%" (
        echo ERROR: Could not clean "%STAGE_DIR%".
        goto :Failed
    )
)
mkdir "%STAGE_DIR%" || goto :Failed

echo.
echo [4/5] Stage runtime files

copy /y "%BUILD_OUTPUT%\%EXE_NAME%" "%STAGE_DIR%\" >nul
if errorlevel 1 (
    echo ERROR: Failed to copy executable.
    goto :Failed
)

if exist "%BUILD_OUTPUT%\*.dll" (
    copy /y "%BUILD_OUTPUT%\*.dll" "%STAGE_DIR%\" >nul
)

if exist "%BUILD_OUTPUT%\*.ini" (
    copy /y "%BUILD_OUTPUT%\*.ini" "%STAGE_DIR%\" >nul
)

call :MirrorDir "%PROJECT_DIR%\Shaders" "%STAGE_DIR%\Shaders" || goto :Failed
call :MirrorDir "%PROJECT_DIR%\Settings" "%STAGE_DIR%\Settings" || goto :Failed
call :MirrorDir "%PROJECT_DIR%\Scripts" "%STAGE_DIR%\Scripts" || goto :Failed

rem Unreal-style staging: copy the cooked runtime roots as roots, then exclude editor/source-only artifacts.
if not exist "%PROJECT_DIR%\Asset" (
    echo ERROR: Required directory was not found: "%PROJECT_DIR%\Asset"
    goto :Failed
)
robocopy "%PROJECT_DIR%\Asset" "%STAGE_DIR%\Asset" /MIR /NFL /NDL /NJH /NJS /NP /XF *.Scene *.umap *.pdb *.ilk *.obj.recipe
if errorlevel 8 (
    echo ERROR: Failed to stage Asset.
    goto :Failed
)

robocopy "%COOKED_SCENE_DIR%" "%STAGE_DIR%\Asset\Content\Scene" /MIR /NFL /NDL /NJH /NJS /NP
if errorlevel 8 (
    echo ERROR: Failed to stage cooked scenes.
    goto :Failed
)

if exist "%PROJECT_DIR%\Data" (
    robocopy "%PROJECT_DIR%\Data" "%STAGE_DIR%\Data" /MIR /NFL /NDL /NJH /NJS /NP /XF *.pdb *.ilk
    if errorlevel 8 (
        echo ERROR: Failed to stage Data.
        goto :Failed
    )
)

echo.
echo [5/5] Verify staged build
call :RequireFile "%STAGE_DIR%\%EXE_NAME%" || goto :Failed
call :RequireDir "%STAGE_DIR%\Shaders" || goto :Failed
call :RequireDir "%STAGE_DIR%\Asset\Content" || goto :Failed
call :RequireDir "%STAGE_DIR%\Settings" || goto :Failed
call :RequireDir "%STAGE_DIR%\Scripts" || goto :Failed
call :RequireFile "%STAGE_DIR%\Settings\Resource\ProjectResourcePaths.ini" || goto :Failed

set /a UMAPPED=0
for /r "%STAGE_DIR%\Asset\Content\Scene" %%F in (*.umap) do set /a UMAPPED+=1
if !UMAPPED! EQU 0 (
    echo ERROR: No cooked .umap files were staged.
    goto :Failed
)

set /a SCENE_JSON=0
for /r "%STAGE_DIR%\Asset\Content\Scene" %%F in (*.Scene) do set /a SCENE_JSON+=1
if !SCENE_JSON! GTR 0 (
    echo ERROR: Editor .Scene files were staged into ShippingBuild.
    goto :Failed
)

echo.
echo ============================================
echo  Build complete: %STAGE_DIR%
echo  Cooked maps staged: !UMAPPED!
echo ============================================
echo.
call :PauseIfNeeded
exit /b 0

:FindVisualStudio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe was not found.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo ERROR: Visual Studio was not found.
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo
exit /b %ERRORLEVEL%

:MirrorDir
set "SRC=%~1"
set "DST=%~2"
if not exist "%SRC%" (
    echo ERROR: Required directory was not found: "%SRC%"
    exit /b 1
)
robocopy "%SRC%" "%DST%" /MIR /NFL /NDL /NJH /NJS /NP
if errorlevel 8 (
    echo ERROR: Failed to stage "%SRC%" to "%DST%".
    exit /b 1
)
exit /b 0

:RequireFile
if not exist "%~1" (
    echo ERROR: Required file is missing: "%~1"
    exit /b 1
)
exit /b 0

:RequireDir
if not exist "%~1\" (
    echo ERROR: Required directory is missing: "%~1"
    exit /b 1
)
exit /b 0

:PauseIfNeeded
if "%NO_PAUSE%"=="0" pause
exit /b 0

:Failed
echo.
echo ============================================
echo  Shipping build failed.
echo ============================================
echo.
call :PauseIfNeeded
exit /b 1
