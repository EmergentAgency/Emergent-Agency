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
        float thisUpdate = millis();    // more accurately calculated time
        float deltaSeconds = (thisUpdate - lastUpdate) / 1000; 
        lastUpdate = thisUpdate;
        logicNode.update(deltaSeconds, nodeSensor.bActive); 
        
        float minLitRange = PI/8; // radians to extend light around locus
        float maxLitRange = PI/2; 
        float rangeRatio;         // 0 => minLitRange, 1 => maxLitRange
        float goingLitRange;

        if (locusActive) {
          for(int i = 0; i < NUM_LEDS_PER_NODE; i++)
          {    // if within max lit range, scale accordingly
              offset = LEDs[i].radPos - loci.radPos;
              if (offset > PI) offset = offset - 2*PI;
              
              // scale light by velocity, max at 1 rad/sec
              rangeRatio = min(LEDs[i].litRatio * abs(loci.v), 1);   
              goingLitRange = minLitRange + (maxLitRange - minLitRange) * rangeRatio;
              
              if (abs(offset) >= goingLitRange) { // if outside of max lit range
                  LEDs[i].litRatio = 0;
              }
              else {
                  LEDs[i].litRatio = 1 - (abs(offset) / goingLitRange);
                  LEDs[i].litRatio = LEDs[i].litRatio * loci.intensity;
                  
                  if (offset * loci.v > 0) {    // if locus moving toward point
                      // LEDs[i].litRatio *= 0.5;  // squish the light on that side -- just looks ugly
                  }
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


// "source" of light along the circle, acts as a damped harmonic oscillator
// LED's are lit based on their proximity to the locus
class Locus
{
    float rad0;    // initial position along circle (in radians)
    float radPos;  // variable position along circle
    // float RGB;  //(?)
    float t;       // time since bounce began
    float v;       // current velocity
    float inflect;      // amplitude of oscillation (not absolute)
    float cutoff = 0.1; // start dying when amplitude gets below this point
    float intensity;    // range intensity, from 0 to 1
    float slow = 10;    // slow down the dying out by this factor, relative to update cycle
    
    // Play with these values. Leave the formulas alone (unless you know differential equations).
    float m = 20;
    float k = 10;               // physical parameters
    float c = 1;
    float x0 = 3*PI/2;           // initial posisiton (rads): where the locus will settle relative to the user
    float v0 = -PI/8;            // initial angular velocity (rads/sec)
    float xFinal;                // final absolute position (rads)
    
    // Keep the damping ratio between 0 and 1, otherwise it will not bounce!
    float eta = c/(2*sqrt(k*m));     // damping ratio
    float w0 = sqrt(k/m);            // natural frequency (rads/sec)
    float wd = w0*sqrt(1-eta*eta);   // natural damped frequency
    float T = 2*PI / sqrt(k*k/(m*m) - c*c/(16*m*m));    // period of oscillation
    
    void init(float inRadPos) {
        rad0 = inRadPos; // initial position = center of activated node
        radPos = rad0;   // start at initial position
        t = 0;
        v = v0;
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
        v = (radPos - oldRadPos) / dT;
        if (oldV * v < 0) { // if going in a different direction  than last time
             inflect = abs(radPos - rad0 + x0);
        }

        println("eta: " + eta + ", radpos: " + radPos + ", v: " + v + ", inflect: " + inflect);
        if (inflect < cutoff) {       // once settled down to near equilibrium point
            intensity -= dT/slow;     // die down gracefully
            if (intensity < 0) {
                die();        
            }     
        }
    }
    
    void die() {        
        locusActive = false; 
    }
}
