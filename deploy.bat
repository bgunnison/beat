@echo off
setlocal

set ROOT=%~dp0

set SRC_DEBUG=%ROOT%build\VST3\Debug\DebugBeat.vst3
set SRC_RELEASE=%ROOT%build\VST3\Release\Beat.vst3
set DST_DIR=C:\ProgramData\vstplugins
set DST_DEBUG=%DST_DIR%\DebugBeat.vst3
set DST_RELEASE=%DST_DIR%\Beat.vst3

if not exist "%SRC_DEBUG%" (
  echo Missing Debug build at: %SRC_DEBUG%
  echo Run build.bat first.
  goto :done
)

if not exist "%SRC_RELEASE%" (
  echo Missing Release build at: %SRC_RELEASE%
  echo Run build.bat first.
  goto :done
)

if not exist "%DST_DIR%" mkdir "%DST_DIR%"

robocopy "%SRC_DEBUG%" "%DST_DEBUG%" /E /NFL /NDL /NJH /NJS /NC /NS
set RC=%ERRORLEVEL%
if %RC% GEQ 8 goto :done

robocopy "%SRC_RELEASE%" "%DST_RELEASE%" /E /NFL /NDL /NJH /NJS /NC /NS
set RC=%ERRORLEVEL%
if %RC% GEQ 8 goto :done

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

echo Deploy OK: %DST_DEBUG% and %DST_RELEASE%

:done
pause
endlocal
