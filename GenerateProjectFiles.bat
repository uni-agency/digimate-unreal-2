@setlocal enabledelayedexpansion

rem Get the path to the Engine directory from the uproject file location
set "PROJECT_DIR=%~dp0"
set "UPROJECT_FILE=%PROJECT_DIR%\%~n0.uproject"

rem Check if the Engine directory path exists in the environment variable
if not defined UE4PATH (
  echo Error: UE4PATH environment variable not set!
  exit /b 1
)

rem Construct the full path to the UnrealBuildTool executable
set "UBT_PATH=%UE4PATH%\Engine\Binaries\DotNET\UnrealBuildTool.exe"

rem Call the UnrealBuildTool with appropriate arguments
"%UBT_PATH%" -projectfiles -project="%UPROJECT_FILE%" -game -rocket -progress

if not errorlevel 1 (
  echo Project files regenerated successfully!
) else (
  echo Error: Failed to regenerate project files!
)

endlocal