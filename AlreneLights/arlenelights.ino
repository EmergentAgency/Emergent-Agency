#include <Adafruit_NeoPixel.h>
#include <avr/power.h>
#include <stdint.h>

#define PIN 6

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
#define NUM_LEDS 240
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

uint8_t fade_R;
uint8_t fade_G;
uint8_t fade_B;

uint8_t max_R;
uint8_t max_G;
uint8_t max_B;

void setup() {
  Serial.begin(9600);
  Serial.println("BOOT");
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
    // Some example procedures showing how to display to the pixels:
  colorWipe(strip.Color(255, 255, 255), 5); // White
  colorWipe(strip.Color(0, 0, 0), 5); // Off
  fade_R = 3;//random(2,6);
  fade_G = 4;//random(2,6);
  fade_B = 5;//random(2,6);
  max_R = random(100,220);
  max_G = random(150,255);
  max_B = random(150,255);  
  Serial.print("MAX:");
  Serial.print((uint8_t)max_R);
  Serial.print(",");
  Serial.print((uint8_t)max_G);
  Serial.print(",");
  Serial.print((uint8_t)max_B);
  Serial.print("\n");
  Serial.print("FADE:");
  Serial.print((uint8_t)fade_R);
  Serial.print(",");
  Serial.print((uint8_t)fade_G);
  Serial.print(",");
  Serial.print((uint8_t)fade_B);
  Serial.print("\n");
}

// Clamp function - float
// Clamps fVal to be within fMin/fMax bounds.
float ClampF(float fVal, float fMin, float fMax)
{
	if(fVal < fMin)
	{
		fVal = fMin;
	}
	else if(fVal > fMax)
	{
		fVal = fMax;
	}

	return fVal;
}

// Clamp function - int
// Clamps iVal to be within iMin/iMax bounds.
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

// Fire
void UpdateSeqFire()
{

	int iMinLEDIndex = 0;
	int iMaxLEDIndex = NUM_LEDS - 1;

	static const int iNumFirePoints = 40;
	static int aFirePoints[iNumFirePoints];
	static int iCyclesTillNewPoints = 0;
	static const int iFirePointsRefreshCycles = 10;
        unsigned long int Drew_RGB; 
        uint8_t Drew_R,Drew_G,Drew_B;

        //Serial.println("UP");
	if(iCyclesTillNewPoints <= 0)  // every 5 cycles
	{
                // Select 40 random pixels with duplicates allowed
		for(int i = 0; i < iNumFirePoints; i++)
		{
			aFirePoints[i] = random(iMinLEDIndex,iMaxLEDIndex);
		}

		iCyclesTillNewPoints = iFirePointsRefreshCycles;
	}
	iCyclesTillNewPoints--;


		for(int i = 0; i < iNumFirePoints; i++)  //For each point in sequence, NOT in strip
		{
			int nLED = aFirePoints[i];
                        // Grab RGB for current LED
                        Drew_RGB = strip.getPixelColor(nLED);  // This is packed as xxRRGGBB
                        Drew_R = (uint8_t)(Drew_RGB>>16);
                        Drew_G = (uint8_t)(Drew_RGB>>8);
                        Drew_B = (uint8_t)(Drew_RGB);
                        // For each color channel, add 1/20 the sequence on color, clamped to the range [0,seq on color]
                    
			Drew_R = ClampI(max_R / 20 + Drew_R, 0, max_R);
			Drew_G = ClampI(max_G / 20 + Drew_G, 0, max_G);
			Drew_B = ClampI(max_B / 20 + Drew_B, 0, max_B);
                        strip.setPixelColor(nLED,Drew_R,Drew_G,Drew_B);	        
		}

	// Fade
        for(int nLED = iMinLEDIndex; nLED < iMaxLEDIndex; nLED++)
        {
                Drew_RGB = strip.getPixelColor(nLED);  // This is packed as xxRRGGBB
                Drew_R = (uint8_t)(Drew_RGB>>16);
                Drew_G = (uint8_t)(Drew_RGB>>8);
                Drew_B = (uint8_t)(Drew_RGB);
                // for every LED in the strip, subtract a fade increment with clamping
                Drew_R = ClampI(Drew_R - fade_R, 0, max_R);
		Drew_G = ClampI(Drew_G - fade_G, 0, max_G);
		Drew_B = ClampI(Drew_B - fade_B, 0, max_B);
                if(false) // debug
                {
                  Serial.print("L0:");
                  Serial.print((int)Drew_R);
                  Serial.print(",");
                  Serial.print((int)Drew_G);
                  Serial.print(",");
                  Serial.print((int)Drew_B);
                  Serial.print("\n");
                }
                // Copy color to output array w/NeoPixel library
                strip.setPixelColor(nLED,Drew_R,Drew_G,Drew_B);	
	}
        strip.show();
}

void loop() {
  UpdateSeqFire();
  delay(5);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
  }
}


