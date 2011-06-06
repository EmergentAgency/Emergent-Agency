/**
 * Emergent Agency node class.  This file contains all the logic for nodes, how to interpret sensor
 * data, which LEDs should be on, and what messages to send and receive.  Nothing platform specific
 * should be in this file so that it can be used in the Processing sim and on the Arduino
 */


class Node
{

// NOTE: this is the only difference I couldn't make the same
// between Processing and Arudino.  You MUST comment out
// the line "public:" in Processing but it MUST be here for Arduino.
//public:

    int index;
    CommunicationLink comLink;
    boolean bSensorActive;
    float totalActiveSensorTime;
    float stayOnTimer;    // total time to stay lit in this state in the sequence
    float timeRemaining;  // time remaining to stay lit
    float radPos;         // position along circle in radians
        
    // back to regular array notation
    // lit ratios for each LED in the node
    float[] LEDs = new float[NUM_LEDS_PER_NODE];

    void init(int inputIndex, CommunicationLink inComLink, float sensRadPos)
    {
        index = inputIndex;
        comLink = inComLink;
        radPos = sensRadPos;
        bSensorActive = false;
        totalActiveSensorTime = 0;
        
        for(int i=0; i < NUM_LEDS_PER_NODE; i++) {
           LEDs[i] = 0; 
        }
    }

    void update(float deltaSeconds, boolean bInSensorActive)
    {
        // save out old sensor value so we can see if anything changed
        boolean bOldSensorActive = bSensorActive;
        bSensorActive = bInSensorActive;
        float litRatio;
        float oldTimeRemaining;

        if(bSensorActive) {
            if (!bOldSensorActive && !locusActive) {  // if newly activated and not already running
                locusActive = true;
                loci.init(radPos);
                //comLink.sendMessage(simple.states[0].nodeON, simple.states[0].timeON);
            }
            //totalActiveSensorTime += deltaSeconds;
        }
    }

    // right now the message is really simple, it is just the number of seconds to stay on for
    void receiveMessage(float message)
    {
        stayOnTimer = message;
        timeRemaining = stayOnTimer;
    }

}
