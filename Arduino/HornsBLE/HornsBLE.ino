/*********************************************************************
 This is an example for our nRF52 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

// This sketch is intended to be used with the NeoPixel control
// surface in Adafruit's Bluefruit LE Connect mobile application.
//
// - Compile and flash this sketch to the nRF52 Feather
// - Open the Bluefruit LE Connect app
// - Switch to the NeoPixel utility
// - Click the 'connect' button to establish a connection and
//   send the meta-data about the pixel layout
// - Use the NeoPixel utility to update the pixels on your device

/* NOTE: This sketch required at least version 1.1.0 of Adafruit_Neopixel !!! */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <bluefruit.h>

#define NEOPIXEL_VERSION_STRING "Neopixel v2.0"
#define PIN                     8   /* Pin used to drive the NeoPixels */

// Number of LEDs
#define NUM_LEDS 20

#define MAXCOMPONENTS  4
uint8_t *pixelBuffer = NULL;
uint8_t width = 0;
uint8_t height = 0;
uint8_t stride;
uint8_t componentsValue;
bool is400Hz;
uint8_t components = 3;     // only 3 and 4 are valid values

// Lookup map used to linearize LED brightness
static const unsigned char exp_map[256]={
  0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,
  3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,5,5,5,
  5,5,5,5,5,6,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8,8,9,9,
  9,9,10,10,10,10,10,11,11,11,11,12,12,12,13,13,13,13,
  14,14,14,15,15,15,16,16,16,17,17,18,18,18,19,19,20,
  20,20,21,21,22,22,23,23,24,24,25,26,26,27,27,28,29,
  29,30,31,31,32,33,33,34,35,36,36,37,38,39,40,41,42,
  42,43,44,45,46,47,48,50,51,52,53,54,55,57,58,59,60,
  62,63,64,66,67,69,70,72,74,75,77,79,80,82,84,86,88,
  90,91,94,96,98,100,102,104,107,109,111,114,116,119,
  122,124,127,130,133,136,139,142,145,148,151,155,158,
  161,165,169,172,176,180,184,188,192,196,201,205,210,
  214,219,224,229,234,239,244,250,255
};



// Clamp function - int
int ClampI(int iVal, int iMin, int iMax)
{
	if(iVal < iMin)
	{
		iVal = iMin;
	}
	else if(iVal > iMax)
	{
		iVal = iMax;
	}

	return iVal;
}



// Color class
class Color
{
public:
	Color() : m_yR(0), m_yG(0), m_yB(0)
	{
	};

	Color(byte yR, byte yG, byte yB) : m_yR(yR), m_yG(yG), m_yB(yB)
	{
	};

	void Add(Color oColor)
	{
		m_yR += oColor.m_yR;
		m_yG += oColor.m_yG;
		m_yB += oColor.m_yB;
	}

	void AddClamped(Color oColor)
	{
		m_yR = ClampI(m_yR + oColor.m_yR, 0, 255);
		m_yG = ClampI(m_yG + oColor.m_yG, 0, 255);
		m_yB = ClampI(m_yB + oColor.m_yB, 0, 255);
	}

	void Sub(Color oColor)
	{
		m_yR -= oColor.m_yR;
		m_yG -= oColor.m_yG;
		m_yB -= oColor.m_yB;
	}

	void SubClamped(Color oColor)
	{
		m_yR = ClampI(m_yR - oColor.m_yR, 0, 255);
		m_yG = ClampI(m_yG - oColor.m_yG, 0, 255);
		m_yB = ClampI(m_yB - oColor.m_yB, 0, 255);
	}

	void Lighten(Color oColor)
	{
		if(oColor.m_yR > m_yR) m_yR = oColor.m_yR;
		if(oColor.m_yG > m_yG) m_yG = oColor.m_yG;
		if(oColor.m_yB > m_yB) m_yB = oColor.m_yB;
	}

	void Mult(float fMult)
	{
		m_yR = byte(float(m_yR) * fMult);
		m_yG = byte(float(m_yG) * fMult);
		m_yB = byte(float(m_yB) * fMult);
	}

	byte m_yR;
	byte m_yG;
	byte m_yB;
};



// Light sequence deef
#define MAX_SEQ_SIZE 128 // 64?
struct Sequence
{
	int m_bActive;
	int m_iNumNotes;
	int m_iNumMeasures;
	int* m_aiPattern;
	Color m_oOnColor;
	Color m_oFadeColor;
	Color m_aoOutColors[NUM_LEDS];
	int m_iLastNoteIndex;

	Sequence() : m_bActive(false), m_iNumNotes(4), m_iNumMeasures(1), m_oOnColor(255,255,255), m_oFadeColor(1,1,1), m_iLastNoteIndex(-1)
	{
		memset(m_aoOutColors, 0, NUM_LEDS * sizeof(Color));
	}
};

static int aiSeq3[] = {1,2,3,4,-2};

// Sequences
#define NUM_SEQ 1
struct Sequence g_aSeq[NUM_SEQ];



Adafruit_NeoPixel neopixel = Adafruit_NeoPixel();

// BLE Service
BLEDis  bledis;
BLEUart bleuart;

void setup()
{
  Serial.begin(115200);
  while ( !Serial ) delay(10);   // for nrf52840 with native usb

  Serial.println("Adafruit Bluefruit Neopixel Test");
  Serial.println("--------------------------------");

  Serial.println();
  Serial.println("Please connect using the Bluefruit Connect LE application");
  
  // Config Neopixels
  neopixel.begin();

  // Init Bluefruit
  Bluefruit.begin();
  // Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
  Bluefruit.setTxPower(4);
  Bluefruit.setName("PandaFruit52");
  Bluefruit.setConnectCallback(connect_callback);

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();  

  // Configure and start BLE UART service
  bleuart.begin();

  // Set up and start advertising
  startAdv();
  
  
 	int nSeq = 0;
 
	g_aSeq[nSeq].m_bActive = true;
	g_aSeq[nSeq].m_iNumMeasures = 1;
	g_aSeq[nSeq].m_aiPattern = aiSeq3;
	for(g_aSeq[nSeq].m_iNumNotes = 0; g_aSeq[nSeq].m_iNumNotes < MAX_SEQ_SIZE; g_aSeq[nSeq].m_iNumNotes++)
	{
		if(g_aSeq[nSeq].m_aiPattern[g_aSeq[nSeq].m_iNumNotes] == -2)
			break;
	}
	g_aSeq[nSeq].m_oOnColor = Color(255,220,150);
	g_aSeq[nSeq].m_oFadeColor = Color(1,2,3);
	nSeq++;

}

void startAdv(void)
{  
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  
  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void connect_callback(uint16_t conn_handle)
{
  char central_name[32] = { 0 };
  Bluefruit.Gap.getPeerName(conn_handle, central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);

  Serial.println("Please select the 'Neopixels' tab, click 'Connect' and have fun");
}



// TEMP_CL - Fire
void UpdateSeqFire(struct Sequence &seq)
{
	delay(50);

	int iMinLEDIndex = 0;
	int iMaxLEDIndex = NUM_LEDS - 1;

	static const int iNumFirePoints = (NUM_LEDS - 1) / 5;
	static int aFirePoints[iNumFirePoints];
	static int iCyclesTillNewPoints = 0;
	static const int iFirePointsRefreshCycles = 5;

	if(iCyclesTillNewPoints <= 0)
	{
		for(int i = 0; i < iNumFirePoints; i++)
		{
			aFirePoints[i] = random(iMinLEDIndex,iMaxLEDIndex);
		}

		iCyclesTillNewPoints = iFirePointsRefreshCycles;
	}
	iCyclesTillNewPoints--;

	if(true)
	{
		// TEMP_CL - don't use the pattern just pick some random LEDs in the range from 32-36
		// also add to what is there, don't overwrite
		for(int i = 0; i < iNumFirePoints; i++)
		{
			int nLED = aFirePoints[i];
			seq.m_aoOutColors[nLED].m_yR = ClampI(seq.m_oOnColor.m_yR / 20 + seq.m_aoOutColors[nLED].m_yR, 0, seq.m_oOnColor.m_yR);
			seq.m_aoOutColors[nLED].m_yG = ClampI(seq.m_oOnColor.m_yG / 20 + seq.m_aoOutColors[nLED].m_yG, 0, seq.m_oOnColor.m_yG);
			seq.m_aoOutColors[nLED].m_yB = ClampI(seq.m_oOnColor.m_yB / 20 + seq.m_aoOutColors[nLED].m_yB, 0, seq.m_oOnColor.m_yB);
		}
	}

	// Fade
    for(int nLED = iMinLEDIndex; nLED < iMaxLEDIndex; nLED++)
    {
		seq.m_aoOutColors[nLED].SubClamped(seq.m_oFadeColor);
	}
	
	// Output to LEDs
    for(int nLED = 0; nLED < NUM_LEDS; nLED++)
    {
		Color oOutColor = seq.m_aoOutColors[nLED];
		
		// Remap brightness values
		byte yR = exp_map[oOutColor.m_yR];
		byte yG = exp_map[oOutColor.m_yG];
		byte yB = exp_map[oOutColor.m_yB];
		
		//TEMP_CL
		//Serial.printf("%d,%d,%d  ", yR, yG, yB);

		neopixel.setPixelColor(nLED, yR, yG, yB);
	}
	//TEMP_CL
	//Serial.println();
	
	neopixel.show();
}



void loop()
{
  // Echo received data
  if ( Bluefruit.connected() && bleuart.notifyEnabled() )
  {
    int command = bleuart.read();


    // TEMP_CL - Help ensure we're starting at the begining of a command
    //while(Bluefruit.connected() && bleuart.notifyEnabled() && command != '!') {
    //  command = bleuart.read();
    //}

    switch (command) {
      case 'V': {   // Get Version
          commandVersion();
          break;
        }
  
      case 'S': {   // Setup dimensions, components, stride...
          commandSetup();
          break;
       }

      case 'C': {   // Clear with color
          commandClearColor();
          break;
      }

      case 'B': {   // Set Brightness
          commandSetBrightness();
          break;
      }
            
      case 'P': {   // Set Pixel
          commandSetPixel();
          break;
      }
  
      case 'I': {   // Receive new image
          commandImage();
          break;
       }
    }
  }
  
  UpdateSeqFire(g_aSeq[0]);


  // TEMP_CL
  //neopixel.show();

  /*
  // TEMP_CL
  int r = 255;
  int g = 96;
  int b = 12;
  int flicker;
  int r1;
  int g1;
  int b1;

  int numLEDs = height * width;
  for(int x = 0; x <numLEDs; ++x) {
    flicker = random(0,40);
    r1 = r-flicker;
    g1 = g-flicker;
    b1 = b-flicker;
    if(g1<0) g1=0;
    if(r1<0) r1=0;
    if(b1<0) b1=0;
    neopixel.setPixelColor(x,r1,g1, b1);
  }
  neopixel.show();
  delay(random(50,150));
*/

}

void swapBuffers()
{
  Serial.println("About to show NeoPixel buffer:");
  uint8_t *base_addr = pixelBuffer;
  int pixelIndex = 0;
  byte r, g, b;
  for (int j = 0; j < height; j++)
  {
    for (int i = 0; i < width; i++) {
	  if (true) { // Use color inputs as value for fire effect
		g_aSeq[0].m_oOnColor = Color(*base_addr, *(base_addr+1), *(base_addr+2));
	  }
      else if (components == 3) {
        //neopixel.setPixelColor(pixelIndex, neopixel.Color(*base_addr, *(base_addr+1), *(base_addr+2)));
        r = exp_map[*base_addr];
        g = exp_map[*(base_addr+1)];
        b = exp_map[*(base_addr+2)];
        neopixel.setPixelColor(pixelIndex, neopixel.Color(r, g, b));
        Serial.printf("%d,%d,%d ", *base_addr, *(base_addr+1), *(base_addr+2));
      }
      else {
        neopixel.setPixelColor(pixelIndex, neopixel.Color(*base_addr, *(base_addr+1), *(base_addr+2), *(base_addr+3) ));
      }
      base_addr+=components;
      pixelIndex++;
    }
    pixelIndex += stride - width;   // Move pixelIndex to the next row (take into account the stride)
  }
  Serial.println("");
  neopixel.show();
}

void commandVersion() {
  Serial.println(F("Command: Version check"));
  sendResponse(NEOPIXEL_VERSION_STRING);
}

void commandSetup() {
  Serial.println(F("Command: Setup"));

  width = bleuart.read();
  height = bleuart.read();
  stride = bleuart.read();
  componentsValue = bleuart.read();
  is400Hz = bleuart.read();

  neoPixelType pixelType;
  pixelType = componentsValue + (is400Hz ? NEO_KHZ400 : NEO_KHZ800);

  components = (componentsValue == NEO_RGB || componentsValue == NEO_RBG || componentsValue == NEO_GRB || componentsValue == NEO_GBR || componentsValue == NEO_BRG || componentsValue == NEO_BGR) ? 3:4;
  
  Serial.printf("\tsize: %dx%d\n", width, height);
  Serial.printf("\tstride: %d\n", stride);
  Serial.printf("\tpixelType %d\n", pixelType);
  Serial.printf("\tcomponents: %d\n", components);

  if (pixelBuffer != NULL) {
      delete[] pixelBuffer;
  }

  uint32_t size = width*height;
  pixelBuffer = new uint8_t[size*components];
  neopixel.updateLength(size);
  neopixel.updateType(pixelType);
  neopixel.setPin(PIN);

  // Done
  sendResponse("OK");
}

void commandSetBrightness() {
  Serial.println(F("Command: SetBrightness"));

   // Read value
  uint8_t brightness = bleuart.read();

  // Set brightness
  neopixel.setBrightness(brightness);

  // Refresh pixels
  swapBuffers();

  // Done
  sendResponse("OK");
}

void commandClearColor() {
  Serial.println(F("Command: ClearColor"));

  // Read color
  uint8_t color[MAXCOMPONENTS];
  for (int j = 0; j < components;) {
    if (bleuart.available()) {
      color[j] = bleuart.read();
      j++;
    }
  }

  // Set all leds to color
  int size = width * height;
  uint8_t *base_addr = pixelBuffer;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < components; j++) {
      *base_addr = color[j];
      base_addr++;
    }
  }

  // Swap buffers
  Serial.println(F("ClearColor completed"));
  swapBuffers();


  if (components == 3) {
    Serial.printf("\tclear (%d, %d, %d)\n", color[0], color[1], color[2] );
  }
  else {
    Serial.printf("\tclear (%d, %d, %d, %d)\n", color[0], color[1], color[2], color[3] );
  }
  
  // Done
  sendResponse("OK");
}

void commandSetPixel() {
  Serial.println(F("Command: SetPixel"));

  // Read position
  uint8_t x = bleuart.read();
  uint8_t y = bleuart.read();

  // Read colors
  uint32_t pixelOffset = y*width+x;
  uint32_t pixelDataOffset = pixelOffset*components;
  uint8_t *base_addr = pixelBuffer+pixelDataOffset;
  for (int j = 0; j < components;) {
    if (bleuart.available()) {
      *base_addr = bleuart.read();
      base_addr++;
      j++;
    }
  }

  // Set colors
  uint32_t neopixelIndex = y*stride+x;
  uint8_t *pixelBufferPointer = pixelBuffer + pixelDataOffset;
  uint32_t color;
  if (components == 3) {
    color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
    Serial.printf("\tcolor (%d, %d, %d)\n",*pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
  }
  else {
    color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );
    Serial.printf("\tcolor (%d, %d, %d, %d)\n", *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );    
  }
  neopixel.setPixelColor(neopixelIndex, color);
  neopixel.show();

  // Done
  sendResponse("OK");
}

void commandImage() {
  Serial.printf("Command: Image %dx%d, %d, %d\n", width, height, components, stride);
  
  // Receive new pixel buffer
  int size = width * height;
  uint8_t *base_addr = pixelBuffer;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < components;) {
      if (bleuart.available()) {
        *base_addr = bleuart.read();
        base_addr++;
        j++;
      }
    }

/*
    if (components == 3) {
      uint32_t index = i*components;
      Serial.printf("\tp%d (%d, %d, %d)\n", i, pixelBuffer[index], pixelBuffer[index+1], pixelBuffer[index+2] );
    }
    */
  }

  // Swap buffers
  Serial.println(F("Image received"));
  swapBuffers();

  // Done
  sendResponse("OK");
}

void sendResponse(char const *response) {
    Serial.printf("Send Response: %s\n", response);
    bleuart.write(response, strlen(response)*sizeof(char));
}
