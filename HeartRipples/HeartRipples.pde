/**
 * TEMP_CL 
 */

PShape g_oRippleCircleShape;
 
int g_iNumRipples = 50;
Ripple[] g_aRipples = new Ripple[g_iNumRipples];

int g_iNumRippleSources = 2;
int[] g_aiSourceCounter = new int[g_iNumRippleSources];
int[] g_iSourceDelay = new int[g_iNumRippleSources];
float[] g_fStartX = new float[g_iNumRippleSources];
float[] g_fStartY = new float[g_iNumRippleSources];

 
void setup() {
  size(800, 600);
  background(0);
  
  g_iSourceDelay[0] = 15;
  g_iSourceDelay[1] = 15;
  
  g_oRippleCircleShape = createShape(ELLIPSE,0,0,10,10);
  g_oRippleCircleShape.setFill(color(255, 127));
 
   /*
  g_oRippleCircleShape = createShape();  
  g_oRippleCircleShape.beginShape();
  g_oRippleCircleShape.vertex(0, -50);
  g_oRippleCircleShape.vertex(14, -20);
  g_oRippleCircleShape.vertex(47, -15);
  g_oRippleCircleShape.vertex(23, 7);
  g_oRippleCircleShape.vertex(29, 40);
  g_oRippleCircleShape.vertex(0, 25);
  g_oRippleCircleShape.vertex(-29, 40);
  g_oRippleCircleShape.vertex(-23, 7);
  g_oRippleCircleShape.vertex(-47, -15);
  g_oRippleCircleShape.vertex(-14, -20);
  g_oRippleCircleShape.endShape(CLOSE);
  g_oRippleCircleShape.setFill(false);
  g_oRippleCircleShape.setStroke(color(255,64));
  */
  
  
  for (int i = 0; i < g_iNumRipples; i++)
  {
    g_aRipples[i] = new Ripple();
  }
}

void draw() {
  // Fade out the previous frame
  blendMode(BLEND);
  fill(0, 30);
  noStroke();
  rect(0, 0, width, height);
  //background(0);


  // Add point sources
  for(int i = 0; i < g_iNumRippleSources; i++)
  {
    g_aiSourceCounter[i]--;
    if(g_aiSourceCounter[i] <= 0)
    {
      g_aiSourceCounter[i] = g_iSourceDelay[i];
      startRipple(g_fStartX[i], g_fStartY[i]);
    }
  }

  // Update the ripples
  blendMode(ADD);
  //blendMode(BLEND);
  
  
  noFill();
  strokeWeight(10);
  stroke(64);
  //rectMode(RADIUS); // TEMP_CL
  
  for (int i = 0; i < g_iNumRipples; i++)
  {
    g_aRipples[i].updateAndDraw();
  }
  
  //filter( BLUR, 2 );
}


void mousePressed()
{
  g_fStartX[0] = mouseX;
  g_fStartY[0] = mouseY;
  g_fStartX[1] = width/2 + (width/2 - mouseX);
  g_fStartY[1] = mouseY;
}

void keyPressed()
{
  if(key == 'q')
  {
    g_iSourceDelay[0]--;
  }
  else if(key == 'a')
  {
    g_iSourceDelay[0]++;
  }
  else if(key == 'w')
  {
    g_iSourceDelay[1]--;
  }
  else if(key == 's')
  {
    g_iSourceDelay[1]++;
  }
}
    
    
void startRipple(float fX, float fY)
{
  // Find non active ripple
  int iRippleIndex = -1;
  for (int i = 0; i < g_iNumRipples; i++)
  {
    if(!g_aRipples[i].m_bActive)
    {
      iRippleIndex = i;
      break;
    }
  }
  
  // Start a new ripple if we found an inactive ripple
  if(iRippleIndex > -1)
  {
    g_aRipples[iRippleIndex].startRipple(fX, fY);
  }
}




class Ripple
{
  float m_fCenterX, m_fCenterY;
  float m_fRadius;
  boolean m_bActive;

  Ripple()
  {
    m_bActive = false;
  }
  
  void startRipple(float fCenterX, float fCenterY)
  {
    m_fCenterX = fCenterX;
    m_fCenterY = fCenterY;
    m_fRadius = 1;
    m_bActive = true;
  }
  
  void updateAndDraw()
  {
    /*
    m_fRadius += 2;
    ellipse(m_fCenterX, m_fCenterY, m_fRadius, m_fRadius);
    return;
    */
    
    if(m_bActive)
    {
      m_fRadius += 10;
      
      if(m_fRadius > width*2)
      {
        m_bActive = false;
      }
      
      ellipse(m_fCenterX, m_fCenterY, m_fRadius, m_fRadius);
      //rect(m_fCenterX, m_fCenterY, m_fRadius, m_fRadius);
      
      /*
      pushMatrix();
      translate(m_fCenterX, m_fCenterY);
      scale(m_fRadius/10, m_fRadius/10);
      shape(g_oRippleCircleShape);
      popMatrix();
      */
    }
  }
}  
  