
/*  Original Code: Pulse Sensor Amped 1.4    by Joel Murphy and Yury Gitman   http://www.pulsesensor.com
    Modified by Chris Linder to deal with two sensors

Read Me:
https://github.com/WorldFamousElectronics/PulseSensor_Amped_Arduino/blob/master/README.md   
 ----------------------       ----------------------  ----------------------
*/

//  Variables
#define NUM_SENSORS 2
int g_aiPulsePins[] = {14, 18};		// Pulse Sensor purple wires
int g_aiFadePins[] = {20, 21};		// pin to do fancy classy fading blink at each beat
int g_iFadeRate = 15;				// used to fade LED on with PWM on fadePin
int g_aiCurFade[] = {0, 0};			// Current fade for each of the fade leds

// Volatile Variables, used in the interrupt service routine
typedef struct
{
	volatile int m_iBPM;				// int that holds raw Analog in 0. updated every 2mS
	volatile int m_iSignal;				// holds the incoming raw data
	volatile int m_iIBI = 600;			// int that holds the time interval between beats! Must be seeded! 
	volatile boolean m_bPulse = false;	// "True" when User's live heartbeat is detected. "False" when not a "live beat". 
	volatile boolean m_bQS = false;		// becomes true when Arduoino finds a beat.

	volatile int m_aiRate[10];						// array to hold last ten IBI values
	volatile unsigned long m_iLastBeatTime = 0;		// used to find IBI
	volatile int m_iP = 512;						// used to find peak in pulse wave, seeded
	volatile int m_iT = 512;						// used to find trough in pulse wave, seeded
	volatile int m_iThresh = 525;					// used to find instant moment of heart beat, seeded
	volatile int m_iAmp = 100;						// used to hold amplitude of pulse waveform, seeded
	volatile boolean m_bFirstBeat = true;			// used to seed rate array so we startup with reasonable BPM
	volatile boolean m_bSecondBeat = false;			// used to seed rate array so we startup with reasonable BPM
} HeartRateInfo;

HeartRateInfo g_aHeartRates[2];



void setup()
{
	// pins that will fade to your heartbeat
	for(int i = 0; i < NUM_SENSORS; ++i)
	{
		pinMode(g_aiFadePins[i],OUTPUT); 
	}

	// we agree to talk fast!
	Serial.begin(115200);

	// sets up to read Pulse Sensor signal every 2mS 
	interruptSetup();

	// IF YOU ARE POWERING The Pulse Sensor AT VOLTAGE LESS THAN THE BOARD VOLTAGE, 
	// UN-COMMENT THE NEXT LINE AND APPLY THAT VOLTAGE TO THE A-REF PIN
	//analogReference(EXTERNAL);   
}



void loop()
{
    serialOutput();       

	for(int i = 0; i < NUM_SENSORS; ++i)
	{
		// if a heart beat was found
		if (g_aHeartRates[i].m_bQS == true)
		{
			// BPM and IBI have been Determined
			// Quantified Self "QS" true when arduino finds a heartbeat

			// Start the LED Fade effect
			g_aiCurFade[i] = 255;

			// A Beat Happened, Output that to serial.     
			serialOutputWhenBeatHappens(i);

			// reset the Quantified Self flag for next time    
			g_aHeartRates[i].m_bQS = false;
		}
	}
  
	// Makes the LED Fade Effect Happen 
	ledsFadeToBeat();

	//  take a break
	delay(20);                             
}



void ledsFadeToBeat()
{
	for(int i = 0; i < NUM_SENSORS; ++i)
	{
		g_aiCurFade[i] -= g_iFadeRate;
		g_aiCurFade[i] = constrain(g_aiCurFade[i],0,255);   //  keep LED fade value from going into negative numbers!
		analogWrite(g_aiFadePins[i], g_aiCurFade[i]);		//  fade LED
	}
}