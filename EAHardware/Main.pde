// logic nodes
Node logicNodes[8];

void setup()
{
}



void loop()
{
    for(int i = 0; i < NUM_NODES; i++)
    {
        float deltaSeconds = 0.03; // TEMP_CL - do this for real
        logicNodes[i].update(deltaSeconds, random(2) == 0);
    }

    delay(500);
}