#include <Tone.h>
Tone tone1;

int NUM_LEDS_PER_NODE = 5;
int NUM_NODES = 8;
int NUM_LEDS = NUM_LEDS_PER_NODE * NUM_NODES;
int SOUND_PIN = 18;


// simple example communication system
class CommunicationLink
{
public:
    void sendMessage(int nodeIndex, float radPos, boolean bClockwise)
    {
    }
};

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
