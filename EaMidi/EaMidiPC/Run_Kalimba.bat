del NodeSettings.txt
copy NodeSettings_Kalimba.txt NodeSettings.txt
start "" "C:\Users\Chris\Documents\MIDI\Somatone_Kalimba.rns"
timeout /t 3
C:\processing-2.1\processing-java.exe --run --force --sketch="C:\Users\Chris\Documents\EmergentAgencyGit\EaMidi\EaMidiPC" --output=C:\Windows\Temp\Processing
