/////////////////////////////////////////////////////////
///
///  Geiger counter by PM
///
///  V0.1 Initial version
///
/////////////////////////////////////////////////////////

// include the necessary libraries
#include <SPI.h>
//#include <SD.h>
#include <TFT.h>  // Arduino LCD library


#define GEIGER_INTERRUPT 0
//#define GEIGER_EMULATOR 9
//#define GEIGER_EMULATOR_CPM 10000
//#define EMULATE_GEIGER
//#define DEBUG

// count data for a whole minute (60000 milliseconds)
// split into 10 chunks of 6 seconds each
#define PERIOD_LENGTH 60000
#define UPDATES_PER_PERIOD 30

// conversion factor from CPM to uSv/h based on data from Libellium for the SBM-20 tube
// http://www.cooking-hacks.com/index.php/documentation/tutorials/geiger-counter-arduino-radiation-sensor-board
#define CPM_TO_USVH 0.0057

// ===========================================
// Globals
// ===========================================

// pulses in the current subperiod
volatile unsigned long pulses = 0;

unsigned long nmeasures = 0;

// stores the pulses for a set of 6 seconds periods
#define RING_SIZE 15

unsigned long ring[RING_SIZE] = {0};

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
//#define cs   10
//#define dc   9
//#define rst  8

// pin definition for the Leonardo/Micro
#define cs   7
#define dc   0
#define rst  1
#define sd_cs  8

TFT TFTscreen = TFT(cs, dc, rst);

// position of the line on screen
int xPos = 0;

int previousRange = 100;

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
  int range=100;
  if (cpm>100)
    range=300;
  if (cpm>300)
    range=1200;
  if (cpm>1200)
    range=12000;
  if(cpm>12000)
    range=99999;
    
  if (previousRange != range)
  {
    resetScreen();
    previousRange = range;
    return;
  }
    
  int drawHeight = map(cpm, 0, range, 0, TFTscreen.height()/2);
  if (drawHeight> TFTscreen.height()/2) {
    drawHeight=TFTscreen.height()/2;
  } 
#ifdef DEBUG
    Serial.print("[DEBUG]: DRAW HEIGHT "); Serial.println(drawHeight,DEC); 
#endif  
  
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
#ifdef DEBUG
    Serial.print("[DEBUG]: XPOS "); Serial.println(xPos,DEC); 
#endif  
  }   
}

void printCPM(int r, int g, int b )
{
  // set the font color to white
  TFTscreen.stroke(r,g,b);
  // set the font size
  TFTscreen.setTextSize(4);
  // write the text to the top left corner of the screen
  int  printVal = cpm; 
  if (printVal>99999)
    printVal=99999;
  String sensorVal = String(printVal);
 
  sensorVal.toCharArray(sensorPrintout, 6);
  
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
    
    
#ifdef DEBUG
    Serial.print("[DEBUG] POINTER: "); Serial.println(pointer,DEC);
#endif 
    // calculate the moving sum of counts
    cpm = (cpm/(UPDATES_PER_PERIOD/RING_SIZE) - ring[pointer] + pulses) * UPDATES_PER_PERIOD/RING_SIZE;

    // store the current period value
    ring[pointer] = pulses;
  
    nmeasures++;
    
    Serial.print("CPM: "); Serial.println(cpm,DEC);
    Serial.print("MEASURE # "); Serial.print(nmeasures,DEC); Serial.print("; PULSES IN "); Serial.print(PERIOD_LENGTH/UPDATES_PER_PERIOD/1000,DEC);  Serial.print("s: "); Serial.println(pulses,DEC);
    // reset the interrupt counter
    pulses = 0;

    // move the pointer to the next position in the ring
    pointer = (pointer + 1) % RING_SIZE;

    // calculate the uSv/h
    usvh = cpm * CPM_TO_USVH;

//    if (cpm<300)
//    {
        drawGraphCPM(139,69,19);
        printCPM(255,255,255);
        printRadiation(150,150,255);
//    }
//    else
//    {
        //Print in different color. Radiation warning
//        drawGraphCPM(139,69,19);
//        printCPM(178,34,34);
//        printRadiation(178,34,34);
//    }
}

#ifdef EMULATE_GEIGER
void emulateGeigerCount()
{
   digitalWrite(GEIGER_EMULATOR, HIGH);
   delayMicroseconds(700);
   digitalWrite(GEIGER_EMULATOR, LOW);
}
#endif

void setup() {
 
  // initialize the serial port
  Serial.begin(9600);
  
 
  // initialize the display
  TFTscreen.begin();

  xPos=0;
  
  // try to access the SD card. If that fails (e.g.
  // no card present), the setup process will stop.
//  Serial.print(F("Initializing SD card..."));
//  if (!SD.begin(sd_cs)) {
//    Serial.println(F("SD init failed!"));
//  }
  
  welcomeScreen();
  resetScreen();

#ifdef EMULATE_GEIGER 
  pinMode(GEIGER_EMULATOR, OUTPUT);
  digitalWrite(GEIGER_EMULATOR, LOW); 
#endif  
   // allow pulse to trigger interrupt on falling
  attachInterrupt(GEIGER_INTERRUPT, pulse, FALLING);
}

void loop() {
#ifdef EMULATE_GEIGER  
    if (millis()>next_emulated_count)
    {
      next_emulated_count=millis()+PERIOD_LENGTH / GEIGER_EMULATOR_CPM;
      emulateGeigerCount();
    }
#endif
    // check if I have to update the info
    if (millis() > next_update) {
        next_update = millis() + PERIOD_LENGTH / UPDATES_PER_PERIOD;
        update();
    } 
}

