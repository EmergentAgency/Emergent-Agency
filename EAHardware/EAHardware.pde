#include <Tone.h>
Tone tone1;

int NUM_LEDS_PER_NODE = 5;
int NUM_NODES = 8;
int NUM_LEDS = NUM_LEDS_PER_NODE * NUM_NODES;
int SOUND_PIN = 18;

int bounceNode;      // stuff needed by Uart parsers
boolean rotation;
char bounceChar;
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
void sendMessage(int nodeIndex, boolean CW) 
{
    parse_outgoing(nodeIndex, CW);    // sets global var bounceChar with appropriate character
    Uart.print(bounceChar);
}

void parse_outgoing(int node, boolean rotation) {
    switch (rotation) {
      case 1:
        switch (node) {
          case 0:
            bounceChar = 'a';
            break;
          case 1:
            bounceChar = 'b';
            break;
          case 2:
            bounceChar = 'c';
            break;
          case 3:
            bounceChar = 'd';
            break;
          case 4:
            bounceChar = 'e';
            break;
          case 5:
            bounceChar = 'f';
            break;
          case 6:
            bounceChar = 'g';
            break;
          case 7:
            bounceChar = 'h';
            break;
        }
      case 0:
        switch (node) {
          case 0:
            bounceChar = 'A';
            break;
          case 1:
            bounceChar = 'B';
            break;
          case 2:
            bounceChar = 'C';
            break;
          case 3:
            bounceChar = 'D';
            break;
          case 4:
            bounceChar = 'E';
            break;
          case 5:
            bounceChar = 'F';
            break;
          case 6:
            bounceChar = 'G';
            break;
          case 7:
            bounceChar = 'H';
            break;
        }
    }
}
void parse_incoming(char x) {
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

