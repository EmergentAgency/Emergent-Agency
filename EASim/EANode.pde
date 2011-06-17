/**
 * Emergent Agency node class.  This file contains all the logic for nodes, how to interpret sensor
 * data, which LEDs should be on, and what messages to send and receive.  Nothing platform specific
 * should be in this file so that it can be used in the Processing sim and on the Arduino
 */
 
// 6-16-11: Chris, I totally subverted your sounds, in favor of a simple sine tone whose frequency
//   changes with speed of the locus, which jitters like a bug.  I think it sounds pretty alive!

// "source" of light along the circle, acts like a damped harmonic oscillator
class Locus
{
// NOTE: this is the only difference I couldn't make the same
// between Processing and Arudino.  You MUST comment out
// the line "public:" in Processing but it MUST be here for Arduino.
//public:

    boolean bActive;
    float inflect;      // amplitude of oscillation 
    float intensity;    // intensity, ranges from 0 to 1
    
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
    float x;       // current position (relative to final resting place, in radians)
    float pureX;   // three variables needed for amplitude calcs (better than using velocity)
    float lastX1;
    float lastX2;
    float v;  // current velocity (start out negative so first bounce will be positive)
    int lastBounceIdx;
    int numBounces;   // used for phase shifts: keeps things from repeating too much
    
    void init() 
    {
        // I played with these values, to keep the bug from ever re-entering the node it started from.
        defaultX0 = PI;     // initial posisiton (rads): where the locus will settle relative to the triggered sensor
        defaultV0 = -PI/2;  // initial angular velocity (rads/sec): how big a kick it gets on each bounce
        eta = 0.11;         // damping ratio (<0.1 means lots of swinging, >=1 means no swinging, <=0 doesn't compute)
        w0 = PI/2;          // natural frequency (rads/sec): how fast it wants to go around
        wd = w0*sqrt(1-eta*eta); // natural frequency, slowed by damping (used in harmonics equations later)
        x0 = defaultX0;
        v0 = defaultV0;         
        v = -v0; // current velocity (start out negative so first bounce will be positive)
        lastBounceIdx = -1;
        numBounces = 0;
    }
    void bounce(float inRadPos, int inSensIndex, boolean inCW)
    {
        bActive = true;
        numBounces++;
        t = 0;
        //println("Bounce from " + inSensIndex + ": w0=" + w0 + ", eta=" + eta);
        rad0 = inRadPos;    // initial position = center of activated node
        radPos = rad0;      // start at initial position
        lastBounceIdx = inSensIndex;
        
        // bounce in opposite direction 
        if (!inCW)
        {
            v = defaultV0;
            v0 = defaultV0;
            x0 = defaultX0;
        }
        else 
        {  
            v = -defaultV0;
            v0 = -defaultV0;
            x0 = -defaultX0;
        }
        CW = inCW;
        x = x0;             // start at initial position
        intensity = 1;      // reset default values
        pureX = x0; 
        lastX1 = x0;
        lastX2 = x0;
        inflect = abs(x0);
    }
    
    void update(float dT) 
    { // pretty harmonics
        t += dT;
        float oldRadPos = radPos;
        float oldX = x;
        float oldV = v;
        
        // add jittery wave to make it look more like a bug
        float jScale = (1-abs(pureX/x0));              // jitters most near equilibrium point
        float phi = (float)lastBounceIdx + numBounces; // change phase shifts for every node, every bounce
        float jitter = jScale*(sin(5*wd*t+phi)+sin(11*wd*t+phi+1)+1.5*(eta*w0*x0 + v0)/wd*sin(1.5*wd*t));  
        // The third term (1.5*(eta*w0*x0 + v0)/wd*sin(1.5*wd*t)) should NOT have a phase shift:
        //  -- it means, "Run away from the triggered node real fast!"
        
        lastX2 = lastX1;
        lastX1 = pureX;
        // underdamped harmonic oscillator equation
        pureX = exp(-1*eta*w0*t) * (x0*cos(wd*t) + (eta*w0*x0 + v0)/wd * sin(wd*t));
        x = pureX + exp(-1*eta*w0*t) * jitter;
        // to see just the jittering, without oscillation or time decay, uncomment the next line:
        // x = jitter;
        radPos = (rad0 - x0 + x);

        // ensure radPos is always [0,TWO_PI)
        while(radPos >= TWO_PI)
            radPos -= TWO_PI;
        while(radPos < 0)
            radPos += TWO_PI;
        
        v = (radPos - oldRadPos) / dT;
        
        if (pureX <= lastX1 && lastX1 >= lastX2) {  // if (un-jittered) direction has changed
           inflect = abs(lastX1);                   // note amplitude of oscillation
           if (inflect > PI)
               inflect = abs(inflect - TWO_PI);     // deal with wraparound (+3*pi/2 = -pi/2)               
        }
        if (oldV * v < 0)       // keep this here: acts more like bug running away
        {                                        
            CW = !CW;
        }

        if (inflect < 0.1) 
        {                                        // when settled down to near equilibrium
            intensity -= dT/10;                      // die down gracefully
            if (intensity < 0)
            {
                bActive = false;                 // reset default values
                numBounces = 0;
                stopTone();
            }
        }
    }
};



class Node
{

// NOTE: this is the only difference I couldn't make the same
// between Processing and Arudino.  You MUST comment out
// the line "public:" in Processing but it MUST be here for Arduino.
//public:

    int index;
    CommunicationLink comLink;
    boolean bSensorActive;
    float radPos;               // position of node along circle (radians)
    Locus loci;                 // light source (okay to have a second class on Arduino ???)

    float maxTimeBetweenNotes;  // in seconds
    float timeTillNextNote;     // in seconds
    float noteTimeStep;         // in seconds

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

        maxTimeBetweenNotes = 0.25;
        timeTillNextNote = 0;
        noteTimeStep = 0.125;
        
        // TEMP - can't use "new" cause arduino can't
        loci = new Locus();        // not needed, if bundling both classes together
        loci.init();

        for(int i=0; i < NUM_LEDS_PER_NODE; i++)
        {
            numLEDoffsets = ((float)i * 2.0) + 1.0;
            LEDradPos = radPos - radOffset + LEDoffset*numLEDoffsets;
            if (LEDradPos < 0) LEDradPos += TWO_PI;
            setLED(i,0); 
            setLEDpos(i,LEDradPos);
            //println("node " + sensIndex + ", LED " + i + ": " + LEDradPos);
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
                comLink.sendMessage(index, radPos, CW);
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
          
          // if loci is in this node, make sound from this speaker
          //if(loci.radPos >= getLEDpos(0) && loci.radPos <= getLEDpos(NUM_LEDS_PER_NODE-1))
          
          // how about a radius variable? check whether loci.radpos is within the node's radius?
          if(index == 0) // TEMP_CL - that wasn't working so I just hard coded this to be node 0
          {
              timeTillNextNote -= deltaSeconds;
              if(timeTillNextNote <= 0)
              {
                  // get and bound speed
                  float maxSpeed = 5.0;
                  float speed = abs(loci.v);
                  if(speed > maxSpeed)
                      speed = maxSpeed;

                  //// shift octave based on speed
                  //int toneFreq = baseNotes[int(random(NUM_BASE_NOTES))] * int(pow(2, int(speed / 2.0)));

                  // shift octave based time since last bounce
                  //int octave = 3 - int(pow(loci.t*2, 0.5));
                  int octave = int(3 * 1/(loci.t*2 + 1));
                  if(octave < 0)
                      octave = 0;
                  int toneFreq = baseNotes[int(random(NUM_BASE_NOTES))] * int(pow(2, octave));

                  // play note
                  //println("TEMP_CL toneFreq=" + toneFreq);
                  //playTone(toneFreq);

                  //// play note random time bounded by maxTimeBetweenNotes and based on octave
                  //timeTillNextNote = random(maxTimeBetweenNotes) / (octave + 1);

                  //// play note next time step
                  //timeTillNextNote = noteTimeStep + timeTillNextNote;

                  // play note random time bounded by maxTimeBetweenNotes and based on speed
                  //timeTillNextNote = random(maxTimeBetweenNotes) / maxSpeed * speed;
                  timeTillNextNote = random(maxTimeBetweenNotes) * (1 - (speed / maxSpeed));
              }
          }
          // set frequency by velocity, gradually increasing with number of bounces (bug gets angrier)
          float desperation  = abs(loci.numBounces * loci.inflect / loci.x0) * 8.0;
          float testFreq = 150.0 + abs(loci.v / loci.v0) * 100.0 + desperation;
          if (testFreq > 3000)          // too high a frequency results in ugly static
            println("v = " + loci.v);
          else
            playTone((int)testFreq);
        }
        else
        {
            for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
                setLED(i,0); // blank LEDs, if no locus
            playTone(0);     // mute sound
        }
    }

    void receiveMessage(int inSensIndex, float inRadPos, boolean CW)
    {
        loci.bounce(inRadPos, inSensIndex, CW);      // bounce the locus! (in the proper direction)
    }
};
