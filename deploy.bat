@echo off
setlocal

set ROOT=%~dp0

set SRC_DEBUG_BIN=%ROOT%build\VST3\Debug\Beat.vst3
set SRC_RELEASE=%ROOT%build\VST3\Release\Beat.vst3
set SRC_UIDESC=%ROOT%vst3\beat.uidesc
set SRC_LOGO=%ROOT%vst3\logo_strip.png
set DST_DIR=C:\ProgramData\vstplugins
set DST_DEBUG=%DST_DIR%\DebugBeat.vst3
set DST_RELEASE=%DST_DIR%\Beat.vst3
set ZIP_RELEASE=%ROOT%Beat.vst3.zip

if not exist "%SRC_DEBUG_BIN%" (
  echo Missing Debug build at: %SRC_DEBUG_BIN%
  echo Run build.bat first.
  goto :done
)

if not exist "%SRC_RELEASE%" (
  echo Missing Release build at: %SRC_RELEASE%
  echo Run build.bat first.
  goto :done
)

if not exist "%SRC_UIDESC%" (
  echo Missing UI desc at: %SRC_UIDESC%
  goto :done
)

if not exist "%SRC_LOGO%" (
  echo Missing logo resource at: %SRC_LOGO%
  goto :done
)

if not exist "%DST_DIR%" mkdir "%DST_DIR%"

if exist "%DST_DEBUG%" (
  rmdir /S /Q "%DST_DEBUG%"
)
if exist "%DST_DEBUG%" (
  echo Debug deploy failed: could not clear %DST_DEBUG%
  echo Close Ableton Live and try again.
  goto :done
)

if exist "%DST_RELEASE%" (
  rmdir /S /Q "%DST_RELEASE%"
)
if exist "%DST_RELEASE%" (
  echo Release deploy failed: could not clear %DST_RELEASE%
  echo Close Ableton Live and try again.
  goto :done
)

robocopy "%SRC_DEBUG_BIN%" "%DST_DEBUG%" /E /NFL /NDL /NJH /NJS /NC /NS
set RC=%ERRORLEVEL%
if %RC% GEQ 8 goto :done

robocopy "%SRC_RELEASE%" "%DST_RELEASE%" /E /NFL /NDL /NJH /NJS /NC /NS
set RC=%ERRORLEVEL%
if %RC% GEQ 8 goto :done

if not exist "%DST_DEBUG%\Contents\Resources" mkdir "%DST_DEBUG%\Contents\Resources"
if not exist "%DST_RELEASE%\Contents\Resources" mkdir "%DST_RELEASE%\Contents\Resources"

copy /Y "%SRC_UIDESC%" "%DST_DEBUG%\Contents\Resources\beat.uidesc" >nul
if errorlevel 1 goto :done
copy /Y "%SRC_LOGO%" "%DST_DEBUG%\Contents\Resources\logo_strip.png" >nul
if errorlevel 1 goto :done

copy /Y "%SRC_UIDESC%" "%DST_RELEASE%\Contents\Resources\beat.uidesc" >nul
if errorlevel 1 goto :done
copy /Y "%SRC_LOGO%" "%DST_RELEASE%\Contents\Resources\logo_strip.png" >nul
if errorlevel 1 goto :done

if not exist "%DST_DEBUG%\Contents\x86_64-win\DebugBeat.vst3" (
  echo Debug deploy failed: missing %DST_DEBUG%\Contents\x86_64-win\DebugBeat.vst3
  goto :done
)

if not exist "%DST_RELEASE%\Contents\x86_64-win\Beat.vst3" (
  echo Release deploy failed: missing %DST_RELEASE%\Contents\x86_64-win\Beat.vst3
  goto :done
)

for %%F in ("%DST_DEBUG%\Contents\Resources\beat.uidesc" "%DST_RELEASE%\Contents\Resources\beat.uidesc") do (
  if not exist "%%~F" (
    echo Deploy failed: missing %%~F
    goto :done
  )
)

for %%F in ("%DST_DEBUG%\Contents\Resources\logo_strip.png" "%DST_RELEASE%\Contents\Resources\logo_strip.png") do (
  if not exist "%%~F" (
    echo Deploy failed: missing %%~F
    goto :done
  )
)

if exist "%ZIP_RELEASE%" del /F /Q "%ZIP_RELEASE%"
if exist "%ZIP_RELEASE%" (
  echo Release zip failed: could not clear %ZIP_RELEASE%
  goto :done
)

powershell -NoProfile -Command "Compress-Archive -Path $env:DST_RELEASE -DestinationPath $env:ZIP_RELEASE -Force"
if errorlevel 1 (
  echo Release zip failed: %ZIP_RELEASE%
  goto :done
)

if not exist "%ZIP_RELEASE%" (
  echo Release zip failed: missing %ZIP_RELEASE%
  goto :done
)

echo Deploy OK: %DST_DEBUG%, %DST_RELEASE%, %ZIP_RELEASE%

:done
pause
endlocal
