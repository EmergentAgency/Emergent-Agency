int NUM_LEDS_PER_NODE = 5;
int NUM_NODES = 8;
int NUM_LEDS = NUM_LEDS_PER_NODE * NUM_NODES;

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
    // TODO - fill in with tone code
}
void stopTone(int nodeIndex)
{
    // TODO - fill in with tone code
}
