/**
 * Emergent Agency sim
 *
 * Chris Linder
 * modified by Charlotte McFarland
 */

// tone playing imports
import ddf.minim.*;
import ddf.minim.signals.*;

// constants
static final int     NUM_NODES = 8;
static final int     NUM_PEOPLE = 8;
static final int     NUM_TICKS_PER_UPDATE = 10;
static final float   SENSOR_SIZE = 200;
static final int     NUM_LEDS_PER_NODE = 5;
static final int     NUM_LEDS = NUM_NODES * NUM_LEDS_PER_NODE;
static final float   LED_SIZE = 15;
static final boolean DRAW_SENSORS = true;
static final boolean PLAY_SOUNDS = true;

// globalTickCount is a super simple way to keep track of time.  This
// is incremented every update loop
int globalTickCount = 0;

// All the people in the sim
Person[] people;

// An index of the person currently being highlighted or dragged around
int personSelected;

// All the simulated nodes which contain sensors and LED and the abstract logic nodes
SimNode[] simNodes;

// last time in milliseconds
int lastMillis;

// simple example communication system
class CommunicationLink
{
    void sendMessage(int nodeIndex, float radPos, boolean bClockwise)
    {
        println("sendMessage:" + nodeIndex);
        for (int i=0; i < NUM_NODES; i++)    // ALL nodes receive message !!
        {
            // Don't send the msg to the node that sent it because wifi
            // communication doesn't work like that
            if(i != nodeIndex)
                simNodes[i].logicNode.receiveMessage(nodeIndex, radPos, bClockwise);
        }
    }
}
CommunicationLink comLink = new CommunicationLink();

// tone vars
Minim minim;
AudioOutput out;
SquareWave[] square;

// tone functions use to abstract tone implementations being different between
// arduino and processing
void playTone(int nodeIndex, int freq)
{
    // play at full volume
    square[nodeIndex].setAmp(1);
    square[nodeIndex].setFreq(freq);
}
void stopTone(int nodeIndex)
{
    // turn off sound
    square[nodeIndex].setAmp(0);
}



// setup function which initializes everything
void setup() 
{
    // create the window with the given size
    size(800, 800);

    // init people
    people = new Person[NUM_PEOPLE];
    for(int i = 0; i < NUM_PEOPLE; i++)
    {
        people[i] = new Person();
        people[i].x = 50;
        people[i].y = 50;
    }

    // init sim nodes that contain sensors and nodes
    simNodes = new SimNode[NUM_NODES];
    for(int i = 0; i < NUM_NODES; i++)
    {
        simNodes[i] = new SimNode();
        simNodes[i].init(i, comLink);
    }

    // setups tones
    minim = new Minim(this);
    // get a line out from Minim, default bufferSize is 1024, default sample rate is 44100, bit depth is 16
    out = minim.getLineOut(Minim.STEREO);


    square = new SquareWave[NUM_NODES];
    for(int i = 0; i < NUM_NODES; i++)
    {
        // create a sine wave Oscillator, set to 440 Hz, at 0.5 amplitude, sample rate from line out
        square[i] = new SquareWave(440, 0.5, out.sampleRate());
        // set the portamento speed on the oscillator to 0 milliseconds so that there is no sliding of tones
        square[i].portamento(10);
        // turn off sound to start
        square[i].setAmp(0);
        // add the oscillator to the line out
        if(PLAY_SOUNDS)
            out.addSignal(square[i]);
    }
}

// shut down
void stop()
{
    // turn off tones
    out.close();
    minim.stop();
    super.stop();
}

// update loop
void draw() 
{
    // calc delta seconds;
    int curMillis = millis();
    int deltaMillis = curMillis - lastMillis;
    lastMillis = curMillis;
    float deltaSeconds = deltaMillis / 1000.0;

    // updates all the nodes
    for(int i = 0; i < NUM_NODES; i++)
    {
        // update sim nodes
        simNodes[i].update(deltaSeconds);
    }

    // sets background color to black
    background(0);
  
    // draws all the sensors (under everything)
    for(int i = 0; i < NUM_NODES; i++)
    {
        if(DRAW_SENSORS)
            simNodes[i].drawSensor();
    }
    
    // handles dragging and drawing of all the people (draw over the sensors, under the LEDs)
    personSelected = -1;
    for(int i = 0; i < NUM_PEOPLE; i++)
    {
        // if we are holding the mouse down on a circle
        if(people[i].locked)
        {
            personSelected = i;

            // Draw white circle
            stroke(255); 
            fill(255, 255, 255);
            ellipse(people[i].x, people[i].y, people[i].size, people[i].size);
        }
           
        // Test if the cursor is over the box 
        else if (mouseX > people[i].x - people[i].size/2 && mouseX < people[i].x + people[i].size/2 && 
                 mouseY > people[i].y - people[i].size/2 && mouseY < people[i].y + people[i].size/2 &&
                 personSelected < 0)
        {
            people[i].bOver = true;
            personSelected = i;
            
            // Draw the circle with white edge
            stroke(255); 
            fill(153);
            ellipse(people[i].x, people[i].y, people[i].size, people[i].size);
        }
        else
        {
            people[i].bOver = false;

            // Draw a grey circle
            stroke(153);
            fill(153);
            ellipse(people[i].x, people[i].y, people[i].size, people[i].size);
        }
     }

    // draws all the LEDs (over everything)
    for(int i = 0; i < NUM_NODES; i++)
    {
        simNodes[i].drawLEDs();
    }
}



// handle mouse press for hippie dragging
void mousePressed()
{
  // bail if we aren't over anything
  if(personSelected < 0)
  {
    return;
  }
  
  if(people[personSelected].bOver) 
  { 
    people[personSelected].locked = true; 
  }
  else
  {
    people[personSelected].locked = false;
  }
  people[personSelected].bdifx = mouseX - people[personSelected].x; 
  people[personSelected].bdify = mouseY - people[personSelected].y;
}



// handle mouse dragged for hippie dragging
void mouseDragged() 
{
  // bail if we aren't over anything
  if(personSelected < 0)
  {
    return;
  }
  
  if(people[personSelected].locked) 
  {
    people[personSelected].x = mouseX - people[personSelected].bdifx; 
    people[personSelected].y = mouseY - people[personSelected].bdify; 
  }
}



// handle mouse released for hippie dragging
void mouseReleased()
{
  // bail if we aren't over anything
  if(personSelected < 0)
  {
    return;
  }
  
  people[personSelected].locked = false;
}

