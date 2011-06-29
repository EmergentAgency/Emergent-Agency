#include <Tone.h>
Tone tone1;

int NUM_LEDS_PER_NODE = 5;
int NUM_NODES = 8;
int NUM_LEDS = NUM_LEDS_PER_NODE * NUM_NODES;
int SOUND_PIN = 18;

int bounceNode;      // stuff needed by Uart parsers
boolean rotation;
char bounceChar;
int lociIdx;
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
// TEMP_CL ignore lociIdx for now
void sendMessage(int nodeIndex, int lociIdx, boolean CW) 
{
    parse_outgoing(nodeIndex, lociIdx, CW);    // sets global var bounceChar with appropriate character
    Uart.print(bounceChar);
}

void parse_outgoing(int node, int lociIdx, boolean rotation)
{
    if(rotation)
    {
        bounceChar = 'a' + lociCharEncodingOffset + node;
    }
    else
    {
        bounceChar = 'A' + lociCharEncodingOffset + node;
    }
}

void parse_incoming(char x)
{
    char lociIdx = 0;
    if(x > 'h' || (x < 'a' && x > 'H'))
    {
        lociIdx = 1;
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

