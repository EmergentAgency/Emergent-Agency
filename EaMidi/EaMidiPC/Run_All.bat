:Start

set project_name=Crystal
set project_time=3600
call :RunProject

set project_name=Bela
set project_time=1800
call :RunProject

REM set project_name=Angles
REM set project_time=100
REM call :RunProject

REM set project_name=Didj
REM set project_time=100
REM call :RunProject

set project_name=Kalimba
set project_time=1800
call :RunProject

goto Start



:RunProject

REM Delete the existing settings file and copy the one for this project into its place
del NodeSettings.txt
copy NodeSettings_%project_name%.txt NodeSettings.txt

REM Load up the Reason file for this project and then wait for it to finish loading
start "" "C:\Users\Chris\Documents\MIDI\Somatone_%project_name%.rns"
timeout /t 4

REM REM Start the EaMidiPC processing sketch twice to make sure the settings get pushed to all the nodes
REM start C:\processing-2.1\processing-java.exe --run --force --sketch="C:\Users\Chris\Documents\EmergentAgencyGit\EaMidi\EaMidiPC" --output=C:\Windows\Temp\Processing
REM timeout /t 8
REM tskill java
start C:\processing-2.1\processing-java.exe --run --force --sketch="C:\Users\Chris\Documents\EmergentAgencyGit\EaMidi\EaMidiPC" --output=C:\Windows\Temp\Processing

REM Run the project for a reasonable amount of time
timeout /t %project_time%

REM Kill Reason and processing
tskill reason
tskill java

REM End the subroutine 
exit /b


