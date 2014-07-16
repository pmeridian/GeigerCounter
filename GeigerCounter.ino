
#include <TFT.h>  // Arduino LCD library
#include <SPI.h>

#define GEIGER_INTERRUPT 0
#define GEIGER_EMULATOR 7

// count data for a whole minute (60000 milliseconds)
// split into 10 chunks of 6 seconds each
#define PERIOD_LENGTH 60000
#define UPDATES_PER_PERIOD 20

// conversion factor from CPM to uSv/h based on data from Libellium for the SBM-20 tube
// http://www.cooking-hacks.com/index.php/documentation/tutorials/geiger-counter-arduino-radiation-sensor-board
#define CPM_TO_USVH 0.0057


#define GEIGER_EMULATOR_CPM 10


// ===========================================
// Globals
// ===========================================

// pulses in the current subperiod
volatile unsigned long pulses = 0;

// stores the pulses for a set of 6 seconds periods
unsigned long ring[UPDATES_PER_PERIOD] = {0};

// pointer to the next cell in the ring to update
byte pointer = 0;

// keeps the sum of counts for the ring
unsigned long cpm = 0;
float usvh = 0;

// time of the next update
unsigned long next_update = 0;
unsigned long next_emulated_count = 0;

// during the first minute after a reset, the display shows a "warming up" message
boolean warmup = true;

// pin definition for the Uno
#define cs   10
#define dc   9
#define rst  8

// pin definition for the Leonardo
// #define cs   7
// #define dc   0
// #define rst  1

TFT TFTscreen = TFT(cs, dc, rst);

// position of the line on screen
int xPos = 0;

// char array to print to the screen
char sensorPrintout[10];

void resetScreen()
{
  xPos=0;
     // clear the screen with a pretty color
  TFTscreen.background(250, 16, 200);
   // write the static text to the screen
  // set the font color to white
  TFTscreen.stroke(255, 255, 255);
  // set the font size
  TFTscreen.setTextSize(2);
  // write the text to the top left corner of the screen
  TFTscreen.text("CPM\n",  TFTscreen.width()-35, 16);
  // set the font color to white
  TFTscreen.stroke(150, 150, 255);
  TFTscreen.text("muSv/h\n", TFTscreen.width()-72, 38);

  // set the fill color to grey
  TFTscreen.fill(255,255,224);
  
  // draw a rectangle in the center of screen
  TFTscreen.rect(0, TFTscreen.height()/2, TFTscreen.width(), TFTscreen.height()/2);
  
   // set the font size
  TFTscreen.setTextSize(3);
}

void welcomeScreen()
{
  // clear the screen with a pretty color
  TFTscreen.background(250, 16, 200);
  // write the static text to the screen
  // set the font color to white
  TFTscreen.stroke(255, 255, 255);
  // set the font size
  TFTscreen.setTextSize(2);
  // write the text to the top left corner of the screen
  TFTscreen.text("+Geiger V0.1+\n", 0, 0);
  TFTscreen.text("++  by PM  ++\n", 0, 30);
  TFTscreen.text("++   2014  ++\n", 0, 60);

  delay(2000);
}

void pulse() {
    ++pulses;
}

void drawGraphCPM(int r, int g, int b)
{
  int drawHeight = map(cpm, 0, 1000, 0, TFTscreen.height()/2);  
  // draw a line in a nice color
  TFTscreen.stroke(r,g,b);
  //  TFTscreen.line(xPos, TFTscreen.height()-drawHeight, xPos, TFTscreen.height());
  TFTscreen.line(xPos, TFTscreen.height()-drawHeight, xPos, TFTscreen.height());
  
  // if the graph has reached the screen edge
  // erase the screen and start again
  if (xPos >= TFTscreen.width()) {
    resetScreen();
  }
  else {
    // increment the horizontal position:
    xPos++;
  }   
}

void printCPM(int r, int g, int b )
{
  // set the font color to white
  TFTscreen.stroke(r,g,b);
  // set the font size
  TFTscreen.setTextSize(4);
  // write the text to the top left corner of the screen
  String sensorVal = String(cpm);
  sensorVal.toCharArray(sensorPrintout, 5);
  
  TFTscreen.text(sensorPrintout,0,5);
}

void printRadiation(int r, int g, int b )
{
  // set the font color to white
  TFTscreen.stroke(r,g,b);
  // set the font size
  TFTscreen.setTextSize(2);
  // write the text to the top left corner of the screen
  String sensorVal = String(usvh);
  sensorVal.toCharArray(sensorPrintout, 5);
  
  TFTscreen.text(sensorPrintout,0,38);
}

void update() {

    //clear previous values from the screen
    printCPM(250,16,200);
    printRadiation(250,16,200);
    
    // calculate the moving sum of counts
    cpm = cpm - ring[pointer] + pulses;

    // store the current period value
    ring[pointer] = pulses;
  
    Serial.print("CPM: "); Serial.println(cpm,DEC);
    Serial.print("PULSES: "); Serial.println(pulses,DEC);
    // reset the interrupt counter
    pulses = 0;

    // move the pointer to the next position in the ring
    pointer = (pointer + 1) % UPDATES_PER_PERIOD;

    // calculate the uSv/h
    usvh = cpm * CPM_TO_USVH;

    if (cpm<300)
    {
        printCPM(255,255,255);
        drawGraphCPM(139,69,19);
        printRadiation(150,150,255);
    }
    else
    {
        //Warning
        printCPM(178,34,34);
        printRadiation(178,34,34);
    }
}

void emulateGeigerCount()
{
   digitalWrite(GEIGER_EMULATOR, HIGH);
   delayMicroseconds(700);
   digitalWrite(GEIGER_EMULATOR, LOW);
}

void setup() {
 
  // initialize the serial port
  Serial.begin(9600);
  
 
  // initialize the display
  TFTscreen.begin();

  xPos=0;
  welcomeScreen();
  resetScreen();
 
  pinMode(GEIGER_EMULATOR, OUTPUT);
  digitalWrite(GEIGER_EMULATOR, LOW); 
   // allow pulse to trigger interrupt on rising
  attachInterrupt(GEIGER_INTERRUPT, pulse, RISING);
}

void loop() {
    if (millis()>next_emulated_count)
    {
      next_emulated_count=millis()+PERIOD_LENGTH / GEIGER_EMULATOR_CPM;
      emulateGeigerCount();
    }

    // check if I have to update the info
    if (millis() > next_update) {
        next_update = millis() + PERIOD_LENGTH / UPDATES_PER_PERIOD;
        update();
    } 
}

