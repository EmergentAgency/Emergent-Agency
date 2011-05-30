/**
 * Emergent Agency sim
 *
 * Chris Linder
 * modified by Charlotte McFarland
 */

// constants
static final int   NUM_NODES = 8;
static final int   NUM_PEOPLE = 8;
static final int   NUM_TICKS_PER_UPDATE = 10;
static final float SENSOR_SIZE = 200;
static final int   NUM_LEDS_PER_NODE = 5;
static final int   NUM_LEDS = NUM_NODES * NUM_LEDS_PER_NODE;
static final float LED_SIZE = 20;

// globalTickCount is a super simple way to keep track of time.  This
// is incremented every update loop
int globalTickCount = 0;

// All the people in the sim
Person[] people;

// An index of the person currently being highlighted or dragged around
int personSelected;

// All the simulated nodes which contain sensors and LED and the abstract logic nodes
SimNode[] simNodes;

// simple example communication system
class CommunicationLink
{
    void sendMessage(int nodeIndex, float message)
    {
        println("sendMessage:" + nodeIndex + " " + message);
        simNodes[nodeIndex].logicNode.receiveMessage(message);
    }
}
CommunicationLink comLink = new CommunicationLink();

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
}

void draw() 
{
    // updates all the nodes
    for(int i = 0; i < NUM_NODES; i++)
    {
        simNodes[i].update();
    }

    // sets background color to black
    background(0);
  
    // draws all the sensors (under everything)
    for(int i = 0; i < NUM_NODES; i++)
    {
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

