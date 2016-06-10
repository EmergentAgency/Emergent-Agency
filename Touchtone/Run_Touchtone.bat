:Start

REM Start Reason and Processing
start "" "C:\Users\MiniTone\Documents\Emergent-Agency\Touchtone\Touchtone_Crystal_Touch_Cords_2.rns"
timeout /t 3
start C:\processing-2.2.1\processing-java.exe --run --force --sketch="C:\Users\MiniTone\Documents\Emergent-Agency\InputGraph" --output=C:\Windows\Temp\Processing

REM Let them run for quite a while
timeout /t 3600

REM Force kill the tasks
taskkill /IM reason.exe /F /T
taskkill /IM java.exe /F /T

REM Wait a bit to make sure they are shut down
REM timeout /t 5

REM Restart everything
goto Start

