/*********************************************************************
 BLE control of a dynamic color fading system for a fixed number
 of pixels. Control this with the Bluefruit phone app.
 
 This is based on an example for the  nRF52 based Bluefruit LE module

*********************************************************************/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <bluefruit.h>

#define NEOPIXEL_VERSION_STRING "Neopixel v2.0"
#define PIN      8       // Pin used to drive the NeoPixels
#define NUM_LEDS 20      // Number of LEDs
#define MAXCOMPONENTS  4 // Max number of color components (eg: 3 = RGB, 4 = RGBW)

uint8_t *pixelBuffer = NULL;
uint8_t width = 8;
uint8_t height = 4;
uint8_t stride = width;
uint8_t componentsValue = NEO_GRB;
bool is400Hz = false;
uint8_t components = 3; // only 3 and 4 are valid values (see MAXCOMPONENTS)

// Lookup map used to linearize LED brightness
static const unsigned char exp_map[256] = {
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
int ClampI(int iVal, int iMin, int iMax) {
	if(iVal < iMin) {
		iVal = iMin;
	}
	else if(iVal > iMax) {
		iVal = iMax;
	}
	return iVal;
}



// Color class
class Color {
public:
	Color() : m_yR(0), m_yG(0), m_yB(0) {
	};

	Color(byte yR, byte yG, byte yB) : m_yR(yR), m_yG(yG), m_yB(yB) {
	};

	void Add(Color oColor) {
		m_yR += oColor.m_yR;
		m_yG += oColor.m_yG;
		m_yB += oColor.m_yB;
	}

	void AddClamped(Color oColor) {
		m_yR = ClampI(m_yR + oColor.m_yR, 0, 255);
		m_yG = ClampI(m_yG + oColor.m_yG, 0, 255);
		m_yB = ClampI(m_yB + oColor.m_yB, 0, 255);
	}

	void Sub(Color oColor) {
		m_yR -= oColor.m_yR;
		m_yG -= oColor.m_yG;
		m_yB -= oColor.m_yB;
	}

	void SubClamped(Color oColor) {
		m_yR = ClampI(m_yR - oColor.m_yR, 0, 255);
		m_yG = ClampI(m_yG - oColor.m_yG, 0, 255);
		m_yB = ClampI(m_yB - oColor.m_yB, 0, 255);
	}

	void Lighten(Color oColor) {
		if(oColor.m_yR > m_yR) m_yR = oColor.m_yR;
		if(oColor.m_yG > m_yG) m_yG = oColor.m_yG;
		if(oColor.m_yB > m_yB) m_yB = oColor.m_yB;
	}

	void Mult(float fMult) {
		m_yR = byte(float(m_yR) * fMult);
		m_yG = byte(float(m_yG) * fMult);
		m_yB = byte(float(m_yB) * fMult);
	}

	byte m_yR;
	byte m_yG;
	byte m_yB;
};



// Light sequence deef
#define MAX_SEQ_SIZE 128
struct Sequence {
	int m_bActive;
	int m_iNumNotes;
	int m_iNumMeasures;
	int* m_aiPattern;
	Color m_oOnColor;
	Color m_oFadeColor;
	Color m_aoOutColors[NUM_LEDS];
	int m_iLastNoteIndex;

	Sequence() : m_bActive(false),
	             m_iNumNotes(4),
				 m_iNumMeasures(1),
				 m_oOnColor(255,255,255),
				 m_oFadeColor(1,1,1),
				 m_iLastNoteIndex(-1) {
		memset(m_aoOutColors, 0, NUM_LEDS * sizeof(Color));
	}
};



// Dummy sequence for "fire" setup which doesn't use sequences
static int aiSeq3[] = {1,2,3,4,-2};

// Sequences
#define NUM_SEQ 1
struct Sequence g_aSeq[NUM_SEQ];

// Setup neo pixels
Adafruit_NeoPixel neopixel = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

// BLE Service
BLEDis  bledis;
BLEUart bleuart;



void setup() {
	Serial.begin(115200);
	while ( !Serial ) delay(10);   // for nrf52840 with native usb

	Serial.println("Bluefruit Neopixel Control");
	Serial.println("--------------------------------");
	Serial.println();
	Serial.println("Please connect using the Bluefruit Connect LE application");

	// Config Neopixels
	neopixel.begin();

	// Init Bluefruit
	Bluefruit.begin();
	// Set max power. Accepted values are: -40, -30, -20, -16, -12, -8, -4, 0, 4
	Bluefruit.setTxPower(4);
	Bluefruit.setName("BLE Horns");
	Bluefruit.setConnectCallback(connect_callback);

	// Configure and Start Device Information Service
	bledis.setManufacturer("Adafruit Industries");
	bledis.setModel("Bluefruit Feather52");
	bledis.begin();  

	// Configure and start BLE UART service
	bleuart.begin();

	// Set up and start advertising
	startAdv();

	// Setup the led "sequence" that is just used for the fade effect now
 	int nSeq = 0;
	g_aSeq[nSeq].m_bActive = true;
	g_aSeq[nSeq].m_iNumMeasures = 1;
	g_aSeq[nSeq].m_aiPattern = aiSeq3;
	for(g_aSeq[nSeq].m_iNumNotes = 0; g_aSeq[nSeq].m_iNumNotes < MAX_SEQ_SIZE; g_aSeq[nSeq].m_iNumNotes++) {
		if(g_aSeq[nSeq].m_aiPattern[g_aSeq[nSeq].m_iNumNotes] == -2)
			break;
	}
	g_aSeq[nSeq].m_oOnColor = Color(255,220,150);
	g_aSeq[nSeq].m_oFadeColor = Color(1,2,3);
}



void startAdv(void) {  
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

void connect_callback(uint16_t conn_handle) {
	char central_name[32] = { 0 };
	Bluefruit.Gap.getPeerName(conn_handle, central_name, sizeof(central_name));

	Serial.print("Connected to ");
	Serial.println(central_name);

	Serial.println("Please select the 'Neopixels' tab, click 'Connect' and have fun");
}



// Color fade update call
void UpdateSeqFire(struct Sequence &seq) {
	delay(50);

	int iMinLEDIndex = 0;
	int iMaxLEDIndex = NUM_LEDS - 1;

	static const int iNumFirePoints = (NUM_LEDS - 1) / 5;
	static int aFirePoints[iNumFirePoints];
	static int iCyclesTillNewPoints = 0;
	static const int iFirePointsRefreshCycles = 5;

	if(iCyclesTillNewPoints <= 0) {
		for(int i = 0; i < iNumFirePoints; i++) {
			aFirePoints[i] = random(iMinLEDIndex,iMaxLEDIndex);
		}
		iCyclesTillNewPoints = iFirePointsRefreshCycles;
	}
	iCyclesTillNewPoints--;

	if(true) {
		// TEMP_CL - don't use the pattern just pick some random LEDs in the range from 32-36
		// also add to what is there, don't overwrite
		for(int i = 0; i < iNumFirePoints; i++) {
			int nLED = aFirePoints[i];
			seq.m_aoOutColors[nLED].m_yR = ClampI(seq.m_oOnColor.m_yR / 20 + seq.m_aoOutColors[nLED].m_yR, 0, seq.m_oOnColor.m_yR);
			seq.m_aoOutColors[nLED].m_yG = ClampI(seq.m_oOnColor.m_yG / 20 + seq.m_aoOutColors[nLED].m_yG, 0, seq.m_oOnColor.m_yG);
			seq.m_aoOutColors[nLED].m_yB = ClampI(seq.m_oOnColor.m_yB / 20 + seq.m_aoOutColors[nLED].m_yB, 0, seq.m_oOnColor.m_yB);
		}
	}

	// Fade
    for(int nLED = iMinLEDIndex; nLED < iMaxLEDIndex; nLED++) {
		seq.m_aoOutColors[nLED].SubClamped(seq.m_oFadeColor);
	}
	
	// Output to LEDs
    for(int nLED = 0; nLED < NUM_LEDS; nLED++) {
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



void loop() {
	// Echo received data
	if ( Bluefruit.connected() && bleuart.notifyEnabled() ) {
		int command = bleuart.read();

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
		}
	}

	UpdateSeqFire(g_aSeq[0]);
}

void updateColors() {
	uint8_t *base_addr = pixelBuffer;
	int pixelIndex = 0;
	byte r, g, b;

	// First color defines subtract color
	r = *base_addr;
	g = *(base_addr+1);
	b = *(base_addr+2);
	
	if(r == 0 && b == 0 && g == 0) {
		// This is a special case of no fading
		r = 0; g = 0; b = 0;
	}
	else {
		// Treat anything less than 64 as 64
		r = r < 64 ? 64 : r;
		g = g < 64 ? 64 : g;
		b = b < 64 ? 64 : b;
		
		// Invert values and then only take the 2 bits
		r = 4 - (r >> 6);
		g = 4 - (g >> 6);
		b = 4 - (b >> 6);
	}
			
	// Set the fade color
	g_aSeq[0].m_oFadeColor = Color(r, g, b);
	Serial.printf("FadeColor = (%d,%d,%d)\n", r, g, b);
	
	// Move along to next color
	base_addr += components;

	// Set main color
	g_aSeq[0].m_oOnColor = Color(*base_addr, *(base_addr+1), *(base_addr+2));
	Serial.printf("OnColor = (%d,%d,%d)\n", *base_addr, *(base_addr+1), *(base_addr+2));
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
  //
  //neoPixelType pixelType;
  //pixelType = componentsValue + (is400Hz ? NEO_KHZ400 : NEO_KHZ800);
  //
  //components = (componentsValue == NEO_RGB || componentsValue == NEO_RBG || componentsValue == NEO_GRB || componentsValue == NEO_GBR || componentsValue == NEO_BRG || componentsValue == NEO_BGR) ? 3:4;
  //
  //Serial.printf("\tsize: %dx%d\n", width, height);
  //Serial.printf("\tstride: %d\n", stride);
  //Serial.printf("\tpixelType %d\n", pixelType);
  //Serial.printf("\tcomponents: %d\n", components);
  
  if (pixelBuffer != NULL) {
      delete[] pixelBuffer;
  }
  
  uint32_t size = width*height;
  pixelBuffer = new uint8_t[size*components];
  
  //neopixel.updateLength(size);
  //neopixel.updateType(pixelType);
  //neopixel.setPin(PIN);

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
  updateColors();

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

  // Update the colors
  Serial.println(F("ClearColor completed"));
  updateColors();


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

	// TEMP_CL - I don't think we need to do this
	//// Set colors
	//uint32_t neopixelIndex = y*stride+x;
	//uint8_t *pixelBufferPointer = pixelBuffer + pixelDataOffset;
	//uint32_t color;
	//if (components == 3) {
	//  color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
	//  Serial.printf("\tcolor (%d, %d, %d)\n",*pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2) );
	//}
	//else {
	//  color = neopixel.Color( *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );
	//  Serial.printf("\tcolor (%d, %d, %d, %d)\n", *pixelBufferPointer, *(pixelBufferPointer+1), *(pixelBufferPointer+2), *(pixelBufferPointer+3) );    
	//}
	//neopixel.setPixelColor(neopixelIndex, color);
	//neopixel.show();

	// Update the colors
	updateColors();

	// Done
	sendResponse("OK");
}


void sendResponse(char const *response) {
    Serial.printf("Send Response: %s\n", response);
    bleuart.write(response, strlen(response)*sizeof(char));
}
