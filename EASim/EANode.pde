/**
 * Emergent Agency node class.  This file contains all the logic for nodes, how to interpret sensor
 * data, which LEDs should be on, and what messages to send and receive.  Nothing platform specific
 * should be in this file so that it can be used in the Processing sim and on the Arduino
 */
 
// Tuning params
boolean bUseScaleBasedSounds = true; // switches between scale based tones and smoothy velocity tones
float globalJitterScale = 0.0;       // scales the amount of jitter the loci has

// Scale based tones
// SYNTAX - Arduino vs Processing difference
//int NUM_BASE_NOTES = 6;
//int[] baseNotes = {131, 147, 175, 196, 220, 262 }; // C, D, F, G, A, C
//int NUM_BASE_NOTES = 4;
//int[] baseNotes = {131, 175, 220, 262 }; // C, F, A, C
//int NUM_BASE_NOTES = 3;
//int[] baseNotes = {131, 196, 262 }; // C, G, C
int NUM_BASE_NOTES = 4;
int[] baseNotes = {131, 156, 196, 262 }; // C, Eflat, G, C



// "source" of light along the circle, acts like a damped harmonic oscillator
class Locus
{
// SYNTAX - Arduino vs Processing difference
// NOTE: this is the only difference I couldn't make the same
// between Processing and Arudino.  You MUST comment out
// the line "public:" in Processing but it MUST be here for Arduino.
//public:

    boolean bActive;
    float inflect;      // amplitude of oscillation 
    float intensity;    // intensity, ranges from 0 to 1
    
    float defaultX0;     // initial posisiton (rads): where the locus will settle relative to the triggered sensor
    float defaultV0;  // initial angular velocity (rads/sec): how big a kick it gets on each bounce
    float defaultEta;
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
    int numBounces;         // used for phase shifts: keeps things from repeating exactly every time
    int numTrappedBounces;
    float desperation;      // grows with every bounce, gets reset when locus dies out
    float voice;            // the frequency it produces as it moves
    float jitterScale;      // how much jittering to perform
    
    void init() 
    {
        // I played with these values, to keep the bug from ever re-entering the node it started from.
        defaultX0 = PI;     // initial posisiton (rads): where the locus will settle relative to the triggered sensor
        defaultV0 = -PI/2;  // initial angular velocity (rads/sec): how big a kick it gets on each bounce
        defaultEta = 0.10;  // damping ratio (<0.1 means lots of swinging, >=1 means no swinging, <=0 doesn't compute)
        eta = defaultEta;   
        w0 = PI/2;          // natural frequency (rads/sec): how fast it wants to go around
        wd = w0*sqrt(1-eta*eta); // natural frequency, slowed by damping (used in harmonics equations later)
        x0 = defaultX0;
        v0 = defaultV0;         
        v = -v0; // current velocity (start out negative so first bounce will be positive)
        lastBounceIdx = -1;
        inflect = abs(x0);
        numBounces = 0;
        numTrappedBounces = 0;
        desperation = 0;
        voice = 0;
    }
    void bounce(float inRadPos, int inSensIndex, boolean inCW)
    {
        bActive = true;
        numBounces++;
        t = 0;
        //println("Bounce from " + inSensIndex + ": w0=" + w0 + ", eta=" + eta);
        rad0 = inRadPos;    // initial position = center of activated node
        radPos = rad0;      // start at initial position
        int currentBounceIdx = inSensIndex;
        int offset;
        float currentX0 = defaultX0;
        
        // check for trapped condition: a second bounce before first swing has completed 
        if (lastBounceIdx != -1 && inflect == abs(x0)) {
            numTrappedBounces++;
            if (inCW) 
                offset = lastBounceIdx - currentBounceIdx;
            else 
                offset = currentBounceIdx - lastBounceIdx;
            if (offset < 0)
                offset += NUM_NODES;        // # of nodes between bounces
            // adjust final resting place to be between the bouncing nodes
            if (offset != 0) 
            {
                currentX0 = (float)offset/2.0 * TWO_PI/(float)NUM_NODES;
                eta *= 2;
                if (eta >=1) eta = 0.9;    // eta >= 1 does not work in the equation
                //println("offset = " + offset + ", current x0 = " + currentX0);
            }
        }
        else
        {
            eta = defaultEta;
        }
        lastBounceIdx = currentBounceIdx;   // record current bounce index
        
        // bounce in opposite direction 
        if (!inCW)
        {
            v = defaultV0;
            v0 = defaultV0;
            x0 = currentX0;
        }
        else 
        {  
            v = -defaultV0;
            v0 = -defaultV0;
            x0 = -currentX0;
        }
        CW = inCW;
        x = x0;             // start at initial position
        intensity = 1;      // reset default values
        pureX = x0; 
        lastX1 = x0;
        lastX2 = x0;
        inflect = abs(x0);
        desperation  = abs(numTrappedBounces * inflect / x0);
    }
    
    void update(float dT) 
    { // pretty harmonics
        t += dT;
        float oldRadPos = radPos;
        float oldX = x;
        float oldV = v;
        
        // add jittery wave to make it look more like a bug
        jitterScale = (1-abs(x/x0));              // jitters most near equilibrium point
        float phi = (float)lastBounceIdx + numBounces; // change phase shifts for every node, every bounce
        float largeJitter = sin(1.5*wd*t);    // slight exaggeration of normal bounce
        float mediumJitter = sin(9*wd*t+phi);                      // having this here increases the randomness
        float smallJitter = sin(11*wd*t+phi+1);                    // super buggy jitter
        float antiJitter = 1 - eta;                                // high damping (due to many bounces) means less jittering
        float jitter = jitterScale*antiJitter*(0.3*largeJitter + 0.5*mediumJitter + 2.0*smallJitter);

        // Allow us to easily turn up and down the jitter
        jitter *= globalJitterScale;

        //if (inflect < 0.2)         // don't jitter out of equilibrium if nearly settled
            //jitter = antiJitter*(0.5*mediumJitter + 2.0*smallJitter);  
        // The third term ((eta*w0*x0 + v0)/wd*sin(1.5*wd*t)) should NOT have a phase shift:
        //  -- it means, "Run away from the triggered node real fast!"
        
        lastX2 = lastX1;    // record last two positions, to detect peak amplitude of each oscillation
        lastX1 = pureX;
        // underdamped harmonic oscillator equation
        pureX = exp(-1*eta*w0*t) * (x0*cos(wd*t) + (eta*w0*x0 + v0)/wd * sin(wd*t));
        x = pureX + exp(-1*eta*w0*t) * jitter;
        // to see just the jittering, without oscillation or time decay, uncomment the next line:
        //x = jitter;
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
        if (oldV * v < 0)         // this makes it act super random (potition tracking would be more predictable)
        {                                        
            CW = !CW;
        }
        if (numTrappedBounces > 30)     // this protects it from being trapped too long
        {
            CW = !CW;
            numTrappedBounces = 15;      // it can still get trapped again, but will break out sooner
            eta = 2.0 * defaultEta;
        }
        // bug gets a little higher-pitched with every bounce (not really noticeable until you trap it)
        //desperation  = abs(numTrappedBounces); 
        voice = (200.0 + 450.0*abs(x/x0) + 35.0*abs(v/v0) + 4.0*numTrappedBounces + 2.0*numBounces) * intensity;
        if (voice < 20)  voice = 0;

        if (inflect < 0.2) // * abs(x0) ?? 
        {                                        // when settled down to near equilibrium
            intensity -= dT/5;                      // start dying down gracefully
            numTrappedBounces = min(numTrappedBounces, 10); // let it calm down a bit
            if (intensity < 0.1)                 // when effectively dead
            {
                bActive = false;                 // reset default values
                lastBounceIdx = -1;
                numTrappedBounces = 0;
                numBounces = 0;
                stopTone(0); // TEMP_CL - not sure which index this supposed to be
            }
        }
    }
};



class Node
{

// SYNTAX - Arduino vs Processing difference
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

    // TEMP_CL - hack for second notes right now
    boolean bUpdateSecondNote;

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
        
        // SYNTAX - Arduino vs Processing difference
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

                // send message to all other nodes
                comLink.sendMessage(index, radPos, CW);

                // let ourselves know about the message too
                receiveMessage(index, radPos, CW);
            }
            else   // bounce the locus when it reaches the center of this (active) node from another node
            {
                if (loci.bActive && index != loci.lastBounceIdx)
                {                                                    // if not already bouncing away from this node
                   offset = abs(radPos - loci.radPos);               // find distance from node center to locus
                   if (offset > PI)
                       offset = abs(offset - TWO_PI);                // deal with wraparound (+3*pi/2 = -pi/2)
                   
                   if (offset < 0.5)
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
            loci.update(deltaSeconds); // update locus position, velocity, etc.

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
                if(bUseScaleBasedSounds)
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
                        playTone(index, toneFreq);

                        // TEMP_CL - hack for second notes right now
                        if(bUpdateSecondNote)
                        {
                            int toneFreq2 = baseNotes[int(random(NUM_BASE_NOTES))] * int(pow(2, octave));
                            playTone(index+1, toneFreq2*2);
                        }
                        bUpdateSecondNote = !bUpdateSecondNote;

                        //// play note random time bounded by maxTimeBetweenNotes and based on octave
                        //timeTillNextNote = random(maxTimeBetweenNotes) / (octave + 1);
   
                        // play note next time step
                        int numBeatsToPlay = int((1 - (speed / maxSpeed)) * random(2)) + 1;
                        timeTillNextNote = noteTimeStep * numBeatsToPlay + timeTillNextNote;
   
                        //// play note random time bounded by maxTimeBetweenNotes and based on speed
                        ////timeTillNextNote = random(maxTimeBetweenNotes) / maxSpeed * speed;
                        //timeTillNextNote = random(maxTimeBetweenNotes) * (1 - (speed / maxSpeed));
                    }
                }
                else
                {
                    // set frequency by velocity, gradually increasing with number of bounces (bug gets angrier)
                    //float desperation  = abs(loci.numBounces * loci.inflect / loci.x0) * 8.0;
                    float testFreq = loci.voice;
                    if (testFreq < 2000.0)          // too high a frequency results in ugly static
                    {
                        playTone(index, (int)testFreq);
                    }
                }
            }
        }
        else
        {
            for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
                setLED(i,0); // blank LEDs, if no locus
            stopTone(index);     // mute sound
        }
    }

    void receiveMessage(int inSensIndex, float inRadPos, boolean CW)
    {
        loci.bounce(inRadPos, inSensIndex, CW);      // bounce the locus! (in the proper direction)
    }
};
