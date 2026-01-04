@echo off
setlocal

set ROOT=%~dp0
set SDK=C:\projects\ableplugs\vst3sdk
set SRC=%SDK%\vstgui4
set BUILD=%SDK%\build-uidesc-editor
set EXE=%BUILD%\Debug\UIDescriptionEditorApp\UIDescriptionEditorApp.exe
set UIDESC=%ROOT%vst3\beat.uidesc.in

if not exist "%SRC%\\CMakeLists.txt" (
  echo VSTGUI source not found at %SRC%
  goto :done
)

cmake -S "%SRC%" -B "%BUILD%" -DVSTGUI_UIDESCRIPTIONEDITOR_APPLICATION=ON
if errorlevel 1 goto :done

cmake --build "%BUILD%" --config Debug --target UIDescriptionEditorApp
if errorlevel 1 goto :done

if exist "%EXE%" (
  if exist "%UIDESC%" (
    start "" "%EXE%" "%UIDESC%"
  ) else (
    start "" "%EXE%"
  )
) else (
  echo UIDescriptionEditorApp.exe not found at %EXE%
  echo Check build output under %BUILD%
)

:done
pause
endlocal
