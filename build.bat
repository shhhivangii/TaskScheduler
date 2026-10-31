@echo off
echo Compiling Task Scheduler...
g++ -std=c++11 main.cpp -o TaskScheduler.exe -mwindows -lcomctl32 -lgdi32 -static
if %errorlevel%==0 (echo SUCCESS! Run TaskScheduler.exe) else (echo FAILED - see the error above)
pause