@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo VCVARS_FAILED
    exit /b 1
)
set CMAKE_C_COMPILER=cl
set CMAKE_CXX_COMPILER=cl

if "%~1"=="configure" (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENPREAMP_DEVELOPER_MODE=OFF -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_CXX_FLAGS="/D_USE_MATH_DEFINES"
) else if "%~1"=="build" (
    cmake --build build --config Release --target OpenPreamp_VST3 OpenPreamp_Standalone OpenPreamp_LV2 OpenPreamp_CLAP OpenPreampSmoke OpenPreampADAA
) else if "%~1"=="test" (
    ctest --test-dir build --output-on-failure
) else (
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENPREAMP_DEVELOPER_MODE=OFF -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_CXX_FLAGS="/D_USE_MATH_DEFINES"
    if errorlevel 1 exit /b 1
    cmake --build build --config Release --target OpenPreamp_VST3 OpenPreamp_Standalone OpenPreamp_LV2 OpenPreamp_CLAP OpenPreampSmoke OpenPreampADAA
)
exit /b %errorlevel%
