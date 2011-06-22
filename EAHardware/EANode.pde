/**
 * Emergent Agency node class.  This file contains all the logic for nodes, how to interpret sensor
 * data, which LEDs should be on, and what messages to send and receive.  Nothing platform specific
 * should be in this file so that it can be used in the Processing sim and on the Arduino
 */

// "source" of light along the circle, acts like a damped harmonic oscillator
class Locus
{
// NOTE: this is the only difference I couldn't make the same
// between Processing and Arudino.  You MUST comment out
// the line "public:" in Processing but it MUST be here for Arduino.
public:

    boolean bActive;
    float inflect;      // amplitude of oscillation 
    float intensity;    // intensity, ranges from 0 to 1
    
    // Play with these values. 
    float defaultX0;     // initial posisiton (rads): where the locus will settle relative to the triggered sensor
    float defaultV0;  // initial angular velocity (rads/sec): how big a kick it gets on each bounce
    float eta;         // damping ratio (<0.1 means lots of swinging, >=1 means no swinging, <=0 doesn't compute)
    float w0;          // natural frequency (rads/sec): how fast it wants to go around

    float wd; // natural frequency, slowed by damping (used in harmonics equations later)
    float x0;
    float v0;            
    boolean CW;
    float rad0;    // initial position along circle (in radians)
    float radPos;  // variable position along circle
    float t;       // time since bounce began
    float v; // current velocity (start out negative so first bounce will be positive)
    float xFinal;  // final absolute position (rads)
    int lastBounceIdx;
    
    void init() 
    {
        defaultX0 = PI;     // initial posisiton (rads): where the locus will settle relative to the triggered sensor
        defaultV0 = -PI/2;  // initial angular velocity (rads/sec): how big a kick it gets on each bounce
        eta = 0.05;         // damping ratio (<0.1 means lots of swinging, >=1 means no swinging, <=0 doesn't compute)
        w0 = PI/2;          // natural frequency (rads/sec): how fast it wants to go around
        wd = w0*sqrt(1-eta*eta); // natural frequency, slowed by damping (used in harmonics equations later)
        x0 = defaultX0;
        v0 = defaultV0;            
        v = -v0; // current velocity (start out negative so first bounce will be positive)
        lastBounceIdx = -1;

    }
    void bounce(float inRadPos, int inSensIndex, boolean inCW)
    {
        bActive = true;
        t = 0;
        //println("Bounce from " + inSensIndex + ": w0=" + w0 + ", eta=" + eta);
        rad0 = inRadPos;    // initial position = center of activated node
        radPos = rad0;      // start at initial position
        lastBounceIdx = inSensIndex;
        
        // bounce in opposite direction (also, excess white space hurts my brain)
        if (!inCW)
        {
            v = defaultV0;
            v0 = defaultV0;
            x0 = defaultX0;
            CW = inCW;
        }
        else 
        {  
            v = -defaultV0;
            v0 = -defaultV0;
            x0 = -defaultX0;
            CW = inCW;  
        }
        
        xFinal = rad0 - x0; // end at final position
        intensity = 1;      // reset default values
        inflect = abs(x0);
    }
    
    void update(float dT) 
    { // pretty harmonics
        t += dT;
        float oldRadPos = radPos;
        float oldV = v;
        float x = exp(-1*eta*w0*t) * (x0*cos(wd*t) + (eta*w0*x0 + v0)/wd * sin(wd*t));
        radPos = (rad0 - x0 + x);

        // ensure radPos is always [0,TWO_PI)
        while(radPos >= TWO_PI)
            radPos -= TWO_PI;
        while(radPos < 0)
            radPos += TWO_PI;
        
        v = (radPos - oldRadPos) / dT;
        if (oldV * v < 0) 
        {                                         // if direction of movement has changed
             inflect = abs(radPos - xFinal);                        // note amplitude of oscillation
             if (inflect > PI)
                 inflect = abs(inflect - TWO_PI);     // deal with wraparound (+3*pi/2 = -pi/2)
             CW = !CW;
             //println("amplitude = " + inflect);
        }
        if (inflect < 0.1) 
        {                                        // when settled down to near equilibrium
            intensity -= dT/10;                                     // die down gracefully
            if (intensity < 0)
                bActive = false;        
        }
    }
};



class Node
{

// NOTE: this is the only difference I couldn't make the same
// between Processing and Arudino.  You MUST comment out
// the line "public:" in Processing but it MUST be here for Arduino.
public:

    int index;
    CommunicationLink comLink;
    boolean bSensorActive;
    float radPos;               // position of node along circle (radians)
    Locus loci;                 // light source (okay to have a second class on Arduino ???)

    // Processing and arduino have different array syntax which makes them incompatible.
    // This is super frustrating but can be worked around by making functions that access
    // elements like arrays.
    float LED0; 
    float LED1; 
    float LED2; 
    float LED3; 
    float LED4;

	// get/set LED lit ratios
    float getLED(int i)
    {
        if(i == 0) return LED0;
        if(i == 1) return LED1;
        if(i == 2) return LED2;
        if(i == 3) return LED3;
        if(i == 4) return LED4;
		
		return 0.0;
    }

    void setLED(int i, float newValue)
    {
        if(i == 0) { LED0 = newValue; return; }
        if(i == 1) { LED1 = newValue; return; }
        if(i == 2) { LED2 = newValue; return; }
        if(i == 3) { LED3 = newValue; return; }
        if(i == 4) { LED4 = newValue; return; }
    }

	// position in radians for each LED in the node relative to the whole circle
	// this is saved to avoid extra computation
    float LEDpos0;                      
    float LEDpos1; 
    float LEDpos2; 
    float LEDpos3; 
    float LEDpos4;

    float getLEDpos(int i)
	{
        if(i == 0) return LEDpos0;
        if(i == 1) return LEDpos1;
        if(i == 2) return LEDpos2;
        if(i == 3) return LEDpos3;
        if(i == 4) return LEDpos4;
        return 0;
    }

    void setLEDpos(int i, float newValue) 
	{
        if(i == 0) { LEDpos0 = newValue; return; }
        if(i == 1) { LEDpos1 = newValue; return; }
        if(i == 2) { LEDpos2 = newValue; return; }
        if(i == 3) { LEDpos3 = newValue; return; }
        if(i == 4) { LEDpos4 = newValue; return; }
    }


    void init(int inIndex, CommunicationLink inComLink)
    {
        index = inIndex;
        comLink = inComLink;
        radPos = (float)index / (float)NUM_NODES * 2 * PI;
        bSensorActive = false;
        float LEDradPos;
        float radOffset = 1 / (2*(float)NUM_NODES) * 2 * PI; // 1/2 of sensor radians
        float LEDoffset = 1 / (2*(float)NUM_LEDS) * 2 * PI;  // 1/2 of LED radians
        float numLEDoffsets;
        
        // TEMP - can't use "new" cause arduino can't
        //loci = new Locus();        // not needed, if bundling both classes together
        loci.init();

        for(int i=0; i < NUM_LEDS_PER_NODE; i++)
        {
            numLEDoffsets = ((float)i * 2.0) + 1.0;
            LEDradPos = radPos - radOffset + LEDoffset*numLEDoffsets;
            if (LEDradPos < 0) LEDradPos += TWO_PI;
            setLED(i,0); 
            setLEDpos(i,LEDradPos);
        }
    }

    void update(float deltaSeconds, boolean bInSensorActive)
    {
        boolean bOldSensorActive = bSensorActive;  // check if changed
        bSensorActive = bInSensorActive;
        float maxLitRange = PI/16;  // radians to extend light around locus
        float offset;               // distance between locus and node (or an LED within the node)
        float wasLit;               // note old lit ratio for each LED, for trailing effect
        float litRatio;             // newly calculated lit ratio
        boolean CW;                 // direction of locus (true if clockwise)

        if(bSensorActive)
        {
            if (!bOldSensorActive && !loci.bActive) // if newly activated and no locus running
            {
                CW = (loci.v > 0) ? true : false;      // note direction of bounce, otherwise errors ensue

                //// send message to all other nodes
                //comLink.sendMessage(index, radPos, CW);

                //// let ourselves know about the message too
                //receiveMessage(index, radPos, CW);

                // TEMP_CL - fake a bounce on the far node for testing
                int tempIdx = index+4;
                float tempRadPos = (float)tempIdx / (float)NUM_NODES * 2 * PI;
                receiveMessage(tempIdx, tempRadPos, CW);
            }
            else   // bounce the locus when it reaches the center of this (active) node from another node
            {
                if (loci.bActive && index != loci.lastBounceIdx)
                {                                                    // if not already bouncing away from this node
                   offset = abs(radPos - loci.radPos);               // find distance from node center to locus
                   if (offset > PI)
                       offset = abs(offset - TWO_PI);                // deal with wraparound (+3*pi/2 = -pi/2)
                   
                   if (offset < 0.3)
                   {                                                 // if close to node center (never exactly on center)
                       CW = !loci.CW;                                // note direction of bounce, otherwise errors ensue
                       comLink.sendMessage(index, radPos, CW);       // bounce the locus!
                   }
                }
            }
            //totalActiveSensorTime += deltaSeconds;  // do we need this?  maybe for auto reset after 10 minutes?
        }
        if (loci.bActive)
        {
          loci.update(deltaSeconds); // update locus position

          for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
          {                                                          // if LED within lit range, light accordingly
              offset = abs(getLEDpos(i) - loci.radPos);              // find distance from LED to locus
              if (offset > PI)
                  offset = abs(offset - TWO_PI);                     // deal with wraparound (+3*pi/2 = -pi/2)
              
              wasLit = getLED(i);
              litRatio = max(wasLit - deltaSeconds, 0);              // fade light over time (max lit time = 1 second)
              if (offset >= maxLitRange)
                  setLED(i,litRatio);                                // fade LEDs to blank, if outside max lit range
              else
              {
                  litRatio = max(1 - (offset/maxLitRange),litRatio); // scale LED lights, if within range
                  setLED(i, litRatio * loci.intensity);
              }
              //if (offset > loci.inflect)
              //   setLED(i,0);                // blank LEDs, if outside amplitude of oscillation
           }
        }
        else
        {
            for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
                setLED(i,0); // blank LEDs, if no locus
        }
    }

    void receiveMessage(int inSensIndex, float inRadPos, boolean CW)
    {
        loci.bounce(inRadPos, inSensIndex, CW);      // bounce the locus! (in the proper direction)
    }
};
