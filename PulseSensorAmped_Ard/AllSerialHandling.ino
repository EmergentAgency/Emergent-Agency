
//  All Serial Handling Code, 

void serialOutput()
{
	for(int i = 0; i < NUM_SENSORS; ++i)
	{
		sendDataToSerial(i == 0 ? 'S' : 's', g_aHeartRates[i].m_iSignal);     // goes to sendDataToSerial function
	}        
}



//  Decides How To OutPut BPM and IBI Data
void serialOutputWhenBeatHappens(int i)
{    
	sendDataToSerial(i == 0 ? 'B' : 'b', g_aHeartRates[i].m_iBPM);   // send heart rate with a 'B' prefix
	sendDataToSerial(i == 0 ? 'Q' : 'q', g_aHeartRates[i].m_iIBI);   // send time between beats with a 'Q' prefix
}



//  Sends Data to Pulse Sensor Processing App, Native Mac App, or Third-party Serial Readers. 
void sendDataToSerial(char symbol, int data)
{
	Serial.print(symbol);
	Serial.println(data);
}
