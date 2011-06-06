/**
 * Emergent Agency processing sim helper classes
 */

float SECONDS_TO_STAY_ON_AFTER_SENSOR_TRIGGERS = 1;  // shortened for quick testing
float SECONDS_TO_BLINK_FAR_NODE = 2;

// Class for each person in the sim
class Person
{
    float x;
    float y;
    float size = 50;
    boolean bOver = false;
    boolean locked = false;
    float bdifx = 0.0; 
    float bdify = 0.0;
}

// Class for the sensor.  The state of the sensor is then fed into the node
class Sensor
{
    int index;
    float x;
    float y;
    float radPos; // was distAlongCircle, actually in radians
    float size = SENSOR_SIZE; 
    boolean bActive = false;

    void init(int inIndex)
    {
        index = inIndex;
        radPos = (float)index / (float)NUM_NODES * 2 * PI;

        x = cos(radPos) * width / 3 + width / 2;
        y = sin(radPos) * height / 3 + height / 2;
    }

    void update()
    {
        bActive = false;
        for(int i = 0; i < NUM_PEOPLE; i++)
        {
            if(dist(x, y, people[i].x, people[i].y) < size/2)
            {
                bActive = true;
                break;
            }
        }
    }
}

// class that represents one LED
class LED 
{ 
  int index;
  int sensIndex;  // index of parent sensor
  float x;
  float y;
  float radPos;
  float size = LED_SIZE;  
  boolean bLit = false;
  float litRatio = 0;  // 0 = totally off, 0.5 = half on, 1 = totally on

  void init(int inSensIndex, int LEDindex, float sensRadPos)
  {
    index = LEDindex; 
    sensIndex = inSensIndex;
    
    // get LEDs aligned within sensor boundaries
    float radOffset = 1 / (2*(float)NUM_NODES) * 2 * PI; // 1/2 of sensor radians
    float LEDoffset = 1 / (2*(float)NUM_LEDS) * 2 * PI;  // 1/2 of LED radians
    radPos = (float)index / (float)NUM_LEDS * 2 * PI - radOffset + LEDoffset;
    
    x = cos(radPos) * width / 3 + width / 2;
    y = sin(radPos) * height / 3 + height / 2;
  }
}



// Class the represents a bundled node on the sim.  This
// contains a sensor, some LED's, and a Node that does the actual logic.
class SimNode
{
    Sensor nodeSensor;
    LED[] LEDs = new LED[NUM_LEDS_PER_NODE]; 
    Node logicNode;
    float lastUpdate;
    float maxLitRange = PI/4; // radians to extend light around locus

    void init(int nodeIndex, CommunicationLink inComLink)
    {
        nodeSensor = new Sensor();
        nodeSensor.init(nodeIndex);
        lastUpdate = millis();

        for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
        {
            LEDs[i] = new LED();
            LEDs[i].init(nodeIndex, nodeIndex * NUM_LEDS_PER_NODE + i, nodeSensor.radPos);
        }
        
        logicNode = new Node();
        logicNode.init(nodeIndex, inComLink, nodeSensor.radPos);
    }

    void update()
    {
        float offset;
        nodeSensor.update();
        float deltaSeconds = (millis() - lastUpdate) / 1000; // accurately calculated time
        logicNode.update(deltaSeconds, nodeSensor.bActive); 
        
        if (locusActive) {
          for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
          {    // if within max lit range, scale accordingly
              offset = abs(LEDs[i].radPos - loci.radPos);
              if (offset > PI) offset = abs(offset - 2*PI);
              if (offset >= maxLitRange) {
                  LEDs[i].litRatio = 0;
              }
              else {
                  LEDs[i].litRatio = 1 - (offset / maxLitRange);
              }
          }
          // update the location of the locus
          loci.update(deltaSeconds);
        }
        else {
          for(int i = 0; i < NUM_LEDS_PER_NODE; i++) {
              LEDs[i].litRatio = 0;  
          }
        }

        lastUpdate = millis();
    }

    void drawSensor()
    {
        stroke(0); 
        if(nodeSensor.bActive)
            fill(150, 64, 64);
        else 
            fill(100, 64, 64);
        ellipse(nodeSensor.x, nodeSensor.y, nodeSensor.size, nodeSensor.size);
    }

    void drawLEDs()
    {
        for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
        {
            float fillRatio = LEDs[i].litRatio * 255;
            fill(0,(int) fillRatio,0);
            ellipse(LEDs[i].x, LEDs[i].y, LEDs[i].size, LEDs[i].size); 
        }
    }
}

// class for a single state in a sequence
// right now, just turn on one node at a time, later will have a complete snapshot
// of the field of nodes and all their LED's
class seqState 
{
    int nodeON;  // 0 = node where the sequence started
    float timeON;
}

class Sequence
{
    int initialNode; // node that started the sequence by being activated
    int num_states;
    seqState[] states;
    
    void init() {
         num_states = 2;
         states = new seqState[num_states];
         for (int i = 0; i < num_states; i++) {
             states[i] = new seqState(); 
         }
         
    }
    void begin(int firstIndex) {
         // clunky way of initializing: should have a parent sequence (where node[0] is always the triggered node)
         // and a child sequence with absolute node numbers determined at run-time
         initialNode = firstIndex;
         states[0].nodeON = initialNode;
         states[0].timeON = SECONDS_TO_STAY_ON_AFTER_SENSOR_TRIGGERS;
         
         states[1].nodeON = (NUM_NODES / 2 + initialNode) % NUM_NODES;
         states[1].timeON = SECONDS_TO_BLINK_FAR_NODE;
    }
}

// "source" of light along the circle, acts as a damped harmonic oscillator
// LED's are lit based on their proximity to the locus
class Locus
{
    float rad0; // initial position along circle (in radians)
    float radPos;  // variable position along circle
    // float RGB; //(?)
    float w0;
    float wd;
    float v0;
    float eta;
    float t0;
    float t;
        
    void init(float inRadPos) {
        rad0 = inRadPos; // initial position = center of activated node
        radPos = rad0;   // start at initial position
        t0 = 0;
        t = t0;
        v0 = PI/4;                 // initial velocity (x0 = 0)
        w0 = PI/8;                 // natural frequency (rads/sec)
        eta = 0.1;                 // damping ratio
        wd = w0*sqrt(1-eta*eta);   // natural damped frequency
    }
    
    void update(float dT) {
        // ugly harmonics -- forgive me
        t += dT;
        float oldRadPos = radPos;
        radPos = (rad0 + exp(-1*eta*wd*t) * v0/wd * sin(wd*t)) % TWO_PI;

        println("locus radpos: " + radPos);
            //locusActive = false; 
    }
}
