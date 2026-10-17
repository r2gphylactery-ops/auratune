@echo off
chcp 65001 >nul
echo === Aura Tune: сборка VST3 ===
where cmake >nul 2>nul || (echo [!] CMake не найден. Установите Visual Studio 2022 с "Desktop development with C++" & pause & exit /b 1)

cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :fail
cmake --build build --config Release --target AuraTune_VST3 AuraTune_Standalone --parallel || goto :fail

echo.
echo Готово! Плагин: build\AuraTune_artefacts\Release\VST3\Aura Tune.vst3
echo Копирую в C:\Program Files\Common Files\VST3 (нужны права администратора)...
xcopy /E /I /Y "build\AuraTune_artefacts\Release\VST3\Aura Tune.vst3" "C:\Program Files\Common Files\VST3\Aura Tune.vst3" >nul && echo Установлено. Пересканируйте плагины в DAW. || echo [!] Не удалось скопировать: запустите этот файл от имени администратора или скопируйте папку вручную.
pause
exit /b 0

:fail
echo [!] Ошибка сборки, см. сообщения выше.
pause
exit /b 1
