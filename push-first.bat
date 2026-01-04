@echo off
setlocal

set ROOT=%~dp0
set REMOTE=https://github.com/bgunnison/beat
set BRANCH=main

cd /d "%ROOT%"

git rev-parse --is-inside-work-tree >nul 2>&1
if errorlevel 1 (
  git init
)

git remote get-url origin >nul 2>&1
if errorlevel 1 (
  git remote add origin %REMOTE%
)

git add -A
git commit -m "First working version" >nul 2>&1

git branch -M %BRANCH%
git push -u origin %BRANCH% --force

pause
endlocal
