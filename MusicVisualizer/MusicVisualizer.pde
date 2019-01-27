import processing.sound.*;

FFT fft;
AudioIn in;

// Define how many FFT bands to use (this needs to be a power of two)
//int bands = 256;
//int bands = 512;
int bands = 1024;
//int bands = 2048;

float[] spectrum = new float[bands];



// The relationship between freqency and notes with a traditional western 12 half step scale is:
// freq = 2^(note/12) or freq = 1.059463^note 
// also
// note = 12*log(freq)/log(2) or note = 39.863137 * log(freq)
// 
// If we have n bands, we can fit 39.863137 * log(n)
// Or not... because we want the early ones to actually all be distint

float[] logSpectrum = new float[bands];
int numNotes = 0;

// Map from bands to notes
int[] noteMap = new int[bands];



// TEMP_CL - not sure if I should smooth spectrum or logSpectrum
// Define a smoothing factor which determines how much the spectrums of consecutive
// points in time should be combined to create a smoother visualisation of the spectrum.
// A smoothing factor of 1.0 means no smoothing (only the data from the newest analysis
// is rendered), decrease the factor down towards 0.0 to have the visualisation update
// more slowly, which is easier on the eye.
float smoothingFactor = 0.4;

// Create a vector to store the smoothed spectrum data in
float[] sum = new float[bands];


// Long term running average
float[] baseline = new float[bands];
float baselineSmoothingFactor = 0.001;




// Variables for drawing the spectrum:
// Declare a scaling factor for adjusting the height of the rectangles
float scale = 5;

// Declare a drawing variable for calculating the width of the 
float barWidth;



void setup() {
  size(2048, 1000);
  //pixelDensity(2);
  
  background(255);
  
  // Calculate the width of the rects depending on how many bands we have
  barWidth = width/float(bands);

  // Create an Input stream which is routed into the Amplitude analyzer
  fft = new FFT(this, bands);
  in = new AudioIn(this, 0);
  
  // start the Audio Input
  in.start();
  
  // patch the AudioIn
  fft.input(in);
  
  // Setup the bands to note map
  for(int i = 0; i < bands; i++) {
    int note = floor(39.863137 * log(i+1));
    noteMap[i] = note;
    print(noteMap[i] + ", ");
  }
  println("");
  numNotes = noteMap[bands-1] + 1;
  println(numNotes);
  
  // Squish bands together to fill in empty sections???
}      

void draw() { 
  background(255);
  fill(0, 0, 0);
  
  
  fft.analyze(spectrum);
  
  // Clean out logSpectrum
  for(int i = 0; i < numNotes; i++) { 
  logSpectrum[i] = 0;
  }
  
  // Do smoothing of spectum
  for (int i = 0; i < bands; i++) {
    // Smooth the FFT spectrum data by smoothing factor
    sum[i] += (spectrum[i] - sum[i]) * smoothingFactor;
  }

  
  
  // Fill in logSpectrum
  for(int i = 0; i < bands; i++) { 
    logSpectrum[noteMap[i]] += sum[i];
  }

  // Update baseline based on logSpectrum 
  for(int i = 0; i < numNotes; i++) {
    /*
    baseline = baseline * (1-baselineSmoothingFactor) + spectrum[i]*baselineSmoothingFactor;
    baseline = baseline + -baselineSmoothingFactor * baseline + spectrum[i]*baselineSmoothingFactor;
    baseline = baseline + (spectrum[i] - baseline) * baselineSmoothingFactor;
     
    baseline = baseline * (baselineSmoothingFactor) + spectrum[i]*(1-baselineSmoothingFactor);
    baseline = baseline * baselineSmoothingFactor + spectrum[i] + baselineSmoothingFactor * -spectrum[i]
    baseline = baselineSmoothingFactor*(baseline-spectrum[i]) + spectrum[i]
    */
    baseline[i] += (logSpectrum[i] - baseline[i]) * baselineSmoothingFactor;
  }
   

  
  
  /*
  for(int i = 0; i < bands; i++){
    // The result of the FFT is normalized
    // draw the line for frequency band i scaling it up by 5 to get more amplitude.
    // TEMP_CL line( i, height, i, height - spectrum[i]*height*5 );
  
    // Draw the rectangles, adjust their height using the scale factor
    rect(i*barWidth, height, barWidth, -spectrum[i] * height * scale);
  } 
  */
  
  
  barWidth = width/float(numNotes);
  
  
  /* 
  //Draw spectrum directly
  scale = 50;
  for(int i = 0; i < numNotes; i++){

  
    // Draw the rectangles, adjust their height using the scale factor
    rect(i*barWidth, height, barWidth, -logSpectrum[i] * height * scale);
  } 
  */
  
  
  /*
  // Draw offset from running average
  scale = 0.25;
  for(int i = 0; i < numNotes; i++){
    float ratio = logSpectrum[i]/ (baseline[i]+0.0001);
    
  
    // Draw the rectangles, adjust their height using the scale factor
    rect(i*barWidth, height, barWidth, -ratio * height * scale);
  } 
  */
  
  
  
  
  // Draw in different way
  background(0);
  //blendMode(ADD);
  //noStroke();
  
  int lastNote = 0;
   for(int i = 0; i < numNotes; i++){
     // Skip the buckets we skipped in the log process
     if(logSpectrum[i] == 0) {
       continue;
     }
     
    //float ratio = logSpectrum[i]/ (baseline[i]+0.0001);
    float ratio = logSpectrum[i]/ (baseline[i]+0.00001);
    //scale = 40.0;
    //int intensity = int(ratio * scale);
    scale = 30.0;
    int intensity = int(pow(ratio,1.5) * scale);
    if(intensity > 255) {
      intensity = 255;
    }
    //intensity = 255;
  
    stroke(intensity, intensity, intensity);
    fill(intensity, intensity, intensity);
    
    // Fixed width
    rect(lastNote*barWidth, 0, (i-lastNote)*barWidth, height);
    
    // intensity affects width
    //float barAdjust = intensity / 4.0;
    //rect(lastNote*barWidth - barAdjust, 0, (i-lastNote)*barWidth, height);
   
    lastNote = i;
  } 

}
