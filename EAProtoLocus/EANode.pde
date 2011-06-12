/**
 * Emergent Agency node class.  This file contains all the logic for nodes, how to interpret sensor
 * data, which LEDs should be on, and what messages to send and receive.  Nothing platform specific
 * should be in this file so that it can be used in the Processing sim and on the Arduino
 */
class Node {
// You MUST comment out the line "public:" in Processing but it MUST be here for Arduino.
//public:
    int index;
    CommunicationLink comLink;
    boolean bSensorActive;
    int sensIndex;              // index of parent sensor
    float radPos;               // position of node along circle (radians)
    float totalActiveLocusTime; // might never need to know this, just in case
    boolean locusActive;        
    float locusRadPos;          // position of locus along the circle (radians)
    float locusVelocity;        // speed of locus travel (rads/sec) (+ = CW, - = CCW)
    float locusIntensity;       // 1 = full brightness, 0 = not visible (probably dead)
    int lastBoundIdx = -1;
        
    // workaround for different array syntax b/w Arduino and Processing
    // lit ratios (min=0, max=1) for each LED in the node
    float LED0;  
    float LED1; 
    float LED2; 
    float LED3; 
    float LED4; 
    // get/set LED lit ratios
    float getLED(int i) {
        if(i == 0) return LED0;
        if(i == 1) return LED1;
        if(i == 2) return LED2;
        if(i == 3) return LED3;
        if(i == 4) return LED4;
        // else 
        return 0;
    }
    void setLED(int i, float newValue) {
        if(i == 0) { LED0 = newValue; return; }
        if(i == 1) { LED1 = newValue; return; }
        if(i == 2) { LED2 = newValue; return; }
        if(i == 3) { LED3 = newValue; return; }
        if(i == 4) { LED4 = newValue; return; }
    }
    // position in radians for each LED in the node
    float LEDpos0;  
    float LEDpos1; 
    float LEDpos2; 
    float LEDpos3; 
    float LEDpos4; 
    // get/set LED lit positions
    float getLEDpos(int i) {
        if(i == 0) return LEDpos0;
        if(i == 1) return LEDpos1;
        if(i == 2) return LEDpos2;
        if(i == 3) return LEDpos3;
        if(i == 4) return LEDpos4;
        // else 
        return 0;
    }
    void setLEDpos(int i, float newValue) {
        if(i == 0) { LEDpos0 = newValue; return; }
        if(i == 1) { LEDpos1 = newValue; return; }
        if(i == 2) { LEDpos2 = newValue; return; }
        if(i == 3) { LEDpos3 = newValue; return; }
        if(i == 4) { LEDpos4 = newValue; return; }
    }
    
    void init(int inputIndex, CommunicationLink inComLink, float sensRadPos, int inSensIndex) {
        index = inputIndex;
        comLink = inComLink;
        radPos = sensRadPos;
        sensIndex = inSensIndex;
        bSensorActive = false;
        locusActive = false;
        totalActiveLocusTime = 0;
        float LEDradPos;
        float radOffset = 1 / (2*(float)NUM_NODES) * 2 * PI; // 1/2 of sensor radians
        float LEDoffset = 1 / (2*(float)NUM_LEDS) * 2 * PI;  // 1/2 of LED radians
        float numLEDoffsets;
        
        for(int i=0; i < NUM_LEDS_PER_NODE; i++) {
          numLEDoffsets = ((float)i * 2.0) + 1.0;
          LEDradPos = sensRadPos - radOffset + LEDoffset*numLEDoffsets;
          if (LEDradPos < 0) LEDradPos += TWO_PI;
          setLED(i,0); 
          setLEDpos(i,LEDradPos);
          //println("node " + sensIndex + ", LED " + i + ": " + LEDradPos);
        }
    }

    void update(float deltaSeconds, boolean bInSensorActive) {
        boolean bOldSensorActive = bSensorActive;  // check if changed
        bSensorActive = bInSensorActive;
        float minLitRange = PI/8; // radians to extend light around locus
        float maxLitRange = PI/4; 
        float rangeRatio;         // 0 => minLitRange, 1 => maxLitRange
        float goingLitRange;
        float goingLitRatio;
        float offset;             // distance between the locus and an LED

        if(bSensorActive) {
            if (!bOldSensorActive && !loci.bActive) {  // if newly activated and not already running
                loci.init(radPos, sensIndex);
                //comLink.sendMessage(simple.states[0].nodeON, simple.states[0].timeON);
            }
            else {  // bounce the locus when it reaches the center of this (active) node
                if (loci.bActive && sensIndex != loci.lastBounceIdx)
                { 
                  offset = abs(radPos - loci.radPos);
                  // deal with special case of 0 rads = TWO_PI rads
                  if (offset > PI) offset = abs(offset - TWO_PI);
                  if (offset < 0.5)  { loci.init(radPos, sensIndex); }
                }
            }
            //totalActiveSensorTime += deltaSeconds;
        }
        if (loci.bActive) {
          // update the location of the locus ONLY ONCE PER CYCLE (at node[1]) !!!
          if (sensIndex == 1) loci.update(deltaSeconds);        

          for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
          {    // if within max lit range, scale accordingly
              offset = abs(getLEDpos(i) - loci.radPos);
              // deal with special case of 0 rads = TWO_PI rads
              if (offset > PI) offset = abs(offset - TWO_PI);
              
              // scale light by velocity, max at 1 rad/sec
              rangeRatio = min(getLED(i) * abs(loci.v), 1);   
              goingLitRange = minLitRange + (maxLitRange - minLitRange) * rangeRatio;
              
              if (offset >= goingLitRange) { // if outside of max lit range
                  setLED(i,0);
              }
              else {
                  goingLitRatio = 1 - (offset / goingLitRange);
                  setLED(i, goingLitRatio * loci.intensity);
              }
           }
        }
        else {
          for(int i = 0; i < NUM_LEDS_PER_NODE; i++) {
              setLED(i,0);  
          }
        }
    }

    // possible messages: 
    // - if locus is not running: "I've been activated!  Start the locus!"
    // - if locus is running: "I am active and the locus just hit me!  Bounce the locus!"
    void receiveMessage(float inSensIndex) {

    }

}

// "source" of light along the circle, acts like a damped harmonic oscillator
// LED's are lit based on their proximity to the locus
class Locus
{
// You MUST comment out the line "public:" in Processing but it MUST be here for Arduino.
//public:
    boolean bActive;
    float inflect;      // amplitude of oscillation (not absolute)
    float cutoff = 0.1; // start dying when amplitude gets below this point
    float intensity;    // range intensity, from 0 to 1
    
    // Play with these values. 
    float defaultX0 = PI;     // initial posisiton (rads): where the locus will settle relative to the triggered sensor
    float defaultV0 = -PI/2;  // initial angular velocity (rads/sec): how big a kick it gets on each bounce
    float eta = 0.05;         // damping ratio: keep between 0 and 0.1 for lots of swinging (>1 = no swinging!)
    float w0 = PI/2;            // natural frequency (rads/sec): how fast it wants to go around, barring interference

    float wd = w0*sqrt(1-eta*eta);   // natural damped frequency: DO NOT CHANGE THIS FORMULA !!
    float x0 = defaultX0;
    float v0 = defaultV0;            
    float rad0;    // initial position along circle (in radians)
    float radPos;  // variable position along circle
    float t;       // time since bounce began
    float v = v0;  // current velocity
    float xFinal;                    // final absolute position (rads)
    int lastBounceIdx = -1;
    
    void init(float inRadPos, int inSensIndex) {
        bActive = true;
        //println("Bounce from " + inSensIndex + ": w0=" + w0 + ", eta=" + eta);
        rad0 = inRadPos; // initial position = center of activated node
        radPos = rad0;   // start at initial position
        t = 0;
        lastBounceIdx = inSensIndex;
        
        float previousVelocity = v;
        if(previousVelocity > 0) {
          v = defaultV0;
          v0 = defaultV0;
          x0 = defaultX0;
        }
        else {
          v = -defaultV0;
          v0 = -defaultV0;
          x0 = -defaultX0;
        }
        
        xFinal = rad0 - x0; // end at final position
        intensity = 1;      // reset default values
        inflect = 1;
    }
    
    void update(float dT) { // pretty harmonics !!
        t += dT;
        float oldRadPos = radPos;
        float oldV = v;
        
        // linear equation, ends up wrapped around a circle
        float x = exp(-1*eta*w0*t) * (x0*cos(wd*t) + (eta*w0*x0 + v0)/wd * sin(wd*t));
        radPos = (rad0 - x0 + x) % TWO_PI;
        
        // note amplitude of each oscillation, so we'll know when equilibrium is reached
        v = (radPos - oldRadPos) / dT;
        if (oldV * v < 0) { // if going in a different direction than last update cycle
             inflect = abs(radPos - xFinal);
             // deal with special case of 0 rads = TWO_PI rads
             if (inflect > PI) inflect = abs(inflect - TWO_PI);
             //println("inflect: " + inflect);
        }
        if (inflect < cutoff) {       // once settled down to near equilibrium point
            intensity -= dT/10;       // die down gracefully
            if (intensity < 0) bActive = false;        
        }
    }
}
