#include <Tone.h>
Tone tone1;

int NUM_LEDS_PER_NODE = 5;
int NUM_NODES = 8;
int NUM_LEDS = NUM_LEDS_PER_NODE * NUM_NODES;
int SOUND_PIN = 18;

int bounceNode;      // stuff needed by Uart parsers
boolean rotation;
char bounceChar;
int lociIdxMsg;
char lociCharEncodingOffset = 'i' - 'a';
boolean newBounce;

HardwareSerial Uart = HardwareSerial();

// tone functions use to abstract tone implementations being different between
// arduino and processing
void playTone(int nodeIndex, int freq)
{
    tone1.play(freq);
}
void stopTone(int nodeIndex)
{
    tone1.stop();
}

// communication functions to abstract differences between arduino and processing
void sendMessage(int nodeIndex, int lociIdx, boolean CW) 
{
    parse_outgoing(nodeIndex, lociIdx, CW);    // sets global var bounceChar with appropriate character
    Uart.print(bounceChar);
}

void parse_outgoing(int node, int lociIdx, boolean rotation)
{
    // set capitals or lower case based on rotation
    bounceChar = rotation ? 'a' : 'A';

    // add in node
    bounceChar += node;

    // add in loci index
    bounceChar += lociIdx * lociCharEncodingOffset;
}

void parse_incoming(char x)
{
    lociIdxMsg = 0;
    if(x > 'h' || (x < 'a' && x > 'H'))
    {
        lociIdxMsg = 1;
        x -= lociCharEncodingOffset;
    }

    switch (x) {
      case 'a':
        bounceNode = 0;
        //This means CW
        rotation = 1;
        break;
      case 'b':
        bounceNode = 1;
        rotation = 1;
        break;
      case 'c':
        bounceNode = 2;
        rotation = 1;
        break;
      case 'd':
        bounceNode = 3;
        rotation = 1;
        break;
      case 'e':
        bounceNode = 4;
        rotation = 1;
        break;
      case 'f':
        bounceNode = 5;
        rotation = 1;
        break;
      case 'g':
        bounceNode = 6;
        rotation = 1;
        break;
      case 'h':
        bounceNode = 7;
        rotation = 1;
        break;
      case 'A':
        bounceNode = 0;
        rotation = 0;
        break;
      case 'B':
        bounceNode = 1;
        rotation = 0;
        break;
      case 'C':
        bounceNode = 2;
        rotation = 0;
        break;
      case 'D':
        bounceNode = 3;
        rotation = 0;
        break;
      case 'E':
        bounceNode = 4;
        rotation = 0;
        break;
      case 'F':
        bounceNode = 5;
        rotation = 0;
        break;
      case 'G':
        bounceNode = 6;
        rotation = 0;
        break;
      case 'H':
        bounceNode = 7;
        rotation = 0;
        break;
    }
}



/* 
   This section contains the function check_motion_state(), which is used for will-o'-the-wisps.  
   The void loop is just a demomstration of how it is to be used.  You must include all indicated global 
   variables to get check_motion_state() to run.  Nothing should be passed to the function.  It will 
   return a boolean true or false if there has been movement and the movement decay timer has not 
   run out.  Paramaters that can be adjusted are the motion_persistance_global, which will keep the 
   node hot for longer and motion_filter_global, which will require more movement events to turn
   a node hot.  The integration window is maniputated in the check_motion_state() function so if 
   one wants to manipulate that it needs to be changed in the check_motion_state() as well.
 */


//>>>>>>>>>>>>>>These are the global variables that are needed to run check_motion_state()<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

const int sensor_pin_global = 38; //The pin that reads the motion sensor.

int motion_persistance_global = 3000;  //The time that a node stays "hot" if motion was detected.
int integration_window_length_global = 500;  //The lenght of time that motion is integrated over. This variable is dynaimcally changed depending on the state.
int motion_filter_global = 3; //The number of motion events required in the time window for motion to be registered.
int sensor_threshold_global = 100; //The sensor is read from 0 to 1024.  To filter noise a minimum reading of 100 is required.

unsigned long window_start_time_global = 0; //The time when the window of integration starts.
unsigned long motion_start_time_global = 0; //The time when the last motion event happened.
unsigned long motion_count_global = 0; //The number of motion events.
unsigned long old_motion_count_global = 0; //The number of motion events when the last window ended.

boolean motion_happened_global = false; //True if there were enough motion events in the specified integration window.
boolean keep_alive_global = false; //True when the decay timer has not yet expired.

boolean check_motion_state() {
  
    boolean boolean_out;  // This is a local variable that is returned true if there is movement.

  if (analogRead(sensor_pin_global) > sensor_threshold_global ) {  //Read the sensor and see if it crosses threashold.
    motion_count_global ++;   //Increment the motion count.
    //    digitalWrite(ledpin, HIGH);  //Uncomment to see movement events on ledpin
    if (keep_alive_global == true) {    //If the node is hot, then keep the node hot for a single movement instead of requiring multiple.                            
      motion_start_time_global = millis();
    }
  }
  else {
    //    digitalWrite(ledpin, LOW);  //Uncomment to see movement events on ledpin
  }

  
  if ((millis() - window_start_time_global) > integration_window_length_global) {  //Check the window timer and then look at movement counts over the last window.
    window_start_time_global = millis();  // restart window timer
    
    if ((motion_count_global - old_motion_count_global) > motion_filter_global) {  //If enough motion has happend then go hot.
      motion_happened_global = true;
      motion_start_time_global = millis();
      integration_window_length_global = 1500;   //Set the integration window to much longer (3x) so it's easier to keep the node hot after you've activated it.
      sensor_threshold_global = 10;   //Decrease the sensor threashold when someone is present to make it easy to keep the node hot.
      keep_alive_global = true;  
    }
    else {
      motion_happened_global = false;  //If movement criteria was not met in the last integration window then set motion happened to false.
    }
    motion_count_global = old_motion_count_global;  //reset the old motion count to the new motion count so the math works for the next loop.
  }
  
    if (motion_happened_global == 1) {  //If there was motion then set that here.
    boolean_out = true;
    keep_alive_global = true;
      }
    else {
      if ((millis() - motion_start_time_global) > motion_persistance_global) {  //If there was no motion then see if the decay timer has run out.
        if (keep_alive_global == true) {                                              //If it has then report no movement and reset the sensitivity of the node.
          boolean_out = false;
          keep_alive_global = false;
          integration_window_length_global = 500;
          sensor_threshold_global = 100;  //Turn the sensor filter back up when someone leaves the node.
        }
      }
 }
  return boolean_out;
}

