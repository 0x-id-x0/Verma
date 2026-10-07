@echo off
REM Verma by 0x.id - local Windows build
REM Needs: Visual Studio 2022 (Desktop development with C++), CMake, Git
cmake -B build -A x64 || goto :err
cmake --build build --config Release --target Verma_VST3 Verma_Standalone --parallel || goto :err
echo.
echo Done! Plugin: build\Verma_artefacts\Release\VST3\Verma.vst3
echo Copy Verma.vst3 to C:\Program Files\Common Files\VST3\ then rescan in FL Studio.
pause
exit /b 0
:err
echo Build failed.
pause
exit /b 1
