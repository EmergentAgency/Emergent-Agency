// Interrupt code

IntervalTimer myTimer;
volatile unsigned long g_iSampleCounter = 0;	// used to determine pulse timing



void interruptSetup()
{     
	myTimer.begin(pulseSense, 2000);
} 



// myTimer makes sure that we take a reading every 2 miliseconds
void pulseSense()
{                         
	noInterrupts();			// disable interrupts while we do this
	g_iSampleCounter += 2;	// keep track of the time in mS with this variable

	for(int i = 0; i < NUM_SENSORS; ++i)
	{
		processPulseSignal(g_aiPulsePins[i], &(g_aHeartRates[i]));
	}
	
	interrupts();			// enable interrupts when youre done!
}

void processPulseSignal(int iPulsePin, HeartRateInfo* pHeartRate)
{
	pHeartRate->m_iSignal = analogRead(iPulsePin);			// read the Pulse Sensor 
	int N = g_iSampleCounter - pHeartRate->m_iLastBeatTime;	// monitor the time since the last beat to avoid noise

	// find the peak and trough of the pulse wave
	// avoid dichrotic noise by waiting 3/5 of last IBI
	if(pHeartRate->m_iSignal < pHeartRate->m_iThresh && N > (pHeartRate->m_iIBI/5)*3)
	{
		// T is the trough
		if (pHeartRate->m_iSignal < pHeartRate->m_iT)
		{                        
			pHeartRate->m_iT = pHeartRate->m_iSignal; // keep track of lowest point in pulse wave 
		}
	}

	// thresh condition helps avoid noise
	if(pHeartRate->m_iSignal > pHeartRate->m_iThresh && pHeartRate->m_iSignal > pHeartRate->m_iP)
	{
		pHeartRate->m_iP = pHeartRate->m_iSignal; // P is the peak to keep track of highest point in pulse wave
	}

	// NOW IT'S TIME TO LOOK FOR THE HEART BEAT
	// signal surges up in value every time there is a pulse
	if (N > 250) // avoid high frequency noise
	{                                   
		if ( (pHeartRate->m_iSignal > pHeartRate->m_iThresh) && (pHeartRate->m_bPulse == false) && (N > (pHeartRate->m_iIBI/5)*3) )
		{        
			pHeartRate->m_bPulse = true;							// set the Pulse flag when we think there is a pulse
			pHeartRate->m_iIBI = g_iSampleCounter - pHeartRate->m_iLastBeatTime;	// measure time between beats in mS
			pHeartRate->m_iLastBeatTime = g_iSampleCounter;               // keep track of time for next pulse

			// seed the running total to get a realisitic BPM at startup
			if(pHeartRate->m_bSecondBeat)
			{
				pHeartRate->m_bSecondBeat = false;
				for(int i=0; i<=9; i++)
				{             
					pHeartRate->m_aiRate[i] = pHeartRate->m_iIBI;                      
				}
			}

			if(pHeartRate->m_bFirstBeat)
			{
				pHeartRate->m_bFirstBeat = false;	// clear firstBeat flag
				pHeartRate->m_bSecondBeat = true;	// set the second beat flag
				sei();								// enable interrupts again
				return;								// IBI value is unreliable so discard it
			}   

			// keep a running total of the last 10 IBI values
			word iRunningTotal = 0;				// clear the runningTotal variable    

			// shift data in the rate array
			for(int i=0; i<=8; i++)
			{                
				pHeartRate->m_aiRate[i] = pHeartRate->m_aiRate[i+1];	// and drop the oldest IBI value 
				iRunningTotal += pHeartRate->m_aiRate[i];				// add up the 9 oldest IBI values
			}

			pHeartRate->m_aiRate[9] = pHeartRate->m_iIBI;	// add the latest IBI to the rate array
			iRunningTotal += pHeartRate->m_aiRate[9];		// add the latest IBI to runningTotal
			iRunningTotal /= 10;							// average the last 10 IBI values 
			pHeartRate->m_iBPM = 60000/iRunningTotal;		// how many beats can fit into a minute? that's BPM!
			pHeartRate->m_bQS = true;						// set Quantified Self flag - QS FLAG IS NOT CLEARED INSIDE THIS ISR
		}
	}

	// when the values are going down, the beat is over
	if (pHeartRate->m_iSignal < pHeartRate->m_iThresh && pHeartRate->m_bPulse == true)
	{
		pHeartRate->m_bPulse = false;										// reset the Pulse flag so we can do it again
		pHeartRate->m_iAmp = pHeartRate->m_iP - pHeartRate->m_iT;           // get amplitude of the pulse wave
		pHeartRate->m_iThresh = pHeartRate->m_iAmp/2 + pHeartRate->m_iT;	// set thresh at 50% of the amplitude
		pHeartRate->m_iP = pHeartRate->m_iThresh;							// reset these for next time
		pHeartRate->m_iT = pHeartRate->m_iThresh;
	}

	// if 2.5 seconds go by without a beat
	if (N > 2500)
	{
		pHeartRate->m_iThresh = 512;					// set thresh default
		pHeartRate->m_iP = 512;							// set P default
		pHeartRate->m_iT = 512;							// set T default
		pHeartRate->m_iLastBeatTime = g_iSampleCounter;	// bring the lastBeatTime up to date        
		pHeartRate->m_bFirstBeat = true;				// set these to avoid noise
		pHeartRate->m_bSecondBeat = false;				// when we get the heartbeat back
	}
}
