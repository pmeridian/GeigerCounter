/////////////////////////////////////////////////////////
///
///  Geiger counter by PM
///
///  V0.1 Initial version
///
/////////////////////////////////////////////////////////

// include the necessary libraries
#include <SPI.h>

#include <TFT.h>  // Arduino LCD library
#include <math.h>

#define GEIGER_INTERRUPT 0
//#define GEIGER_EMULATOR 9
//#define GEIGER_EMULATOR_CPM 10000
//#define EMULATE_GEIGER
//#define DEBUG
#define ENABLE_SD_LOG
#ifdef ENABLE_SD_LOG
#include <SD.h>
#endif
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

// counter of measures done since last reset
unsigned long nmeasures = 0;

// moving average ring 
#define RING_SIZE 15
unsigned long ring[RING_SIZE] = {0};
byte pointer = 0;

// keeps the sum of counts for the ring
unsigned long cpm = 0;
float usvh = 0;

// time of the next update
unsigned long next_update = 0;
unsigned long next_emulated_count = 0;

// during the first minute after a reset, the display shows a "warming up" message
//boolean warmup = true;

boolean bckLitIsPressed = false;
short bckLitVal=2;

// pin definition for the Uno
//#define cs   10
//#define dc   9
//#define rst  8

// pin definition for the Leonardo/Micro
#define cs   7
#define dc   0
#define rst  1
#define sd_cs  4
#define LIT_PWM 5
#define bckLitButton 6

//TFTscreen + SDcard
TFT TFTscreen = TFT(cs, dc, rst);


//Parameters for display
#define X_START_POS 31
#define Y_GRAPH_START 40
#define Y_GRAPH_HEIGHT 60
#define N_Y_DIV 3
#define Y_AXIS_VAL_0 0
#define Y_AXIS_VAL_1 500
#define Y_AXIS_VAL_2 4000
//#define Y_AXIS_VAL_3 10000
#define Y_AXIS_VAL_MAX 99999

// position of the line on screen
int xPos = 0;
// char array to print to the screen
char sensorPrintout[10];



void printInt(int val, int length, int x, int y)
{
  String sensorVal = String(val);
  sensorVal.toCharArray(sensorPrintout, length);
  TFTscreen.text(sensorPrintout, x, y);
}

void drawReferenceFrame()
{

  // set the font size
  TFTscreen.setTextSize(1);
  TFTscreen.stroke(0,0,0);
  TFTscreen.text("CPM", 1, (TFTscreen.height()*Y_GRAPH_START/100)+2);

  printInt(Y_AXIS_VAL_0, 6, 25, TFTscreen.height() - valHeight(Y_AXIS_VAL_0) - 8);
  printInt(Y_AXIS_VAL_1, 6, 13, TFTscreen.height() - valHeight(Y_AXIS_VAL_1) - 8);
  printInt(Y_AXIS_VAL_2, 6, 7,  TFTscreen.height() - valHeight(Y_AXIS_VAL_2) - 8);
//  printInt(Y_AXIS_VAL_3, 6, 1,  TFTscreen.height() - valHeight(Y_AXIS_VAL_3) - 8);
  
  TFTscreen.noFill();
  TFTscreen.rect(X_START_POS, TFTscreen.height()*Y_GRAPH_START/100+1, TFTscreen.width()-X_START_POS, TFTscreen.height()*Y_GRAPH_HEIGHT/100-1);
  TFTscreen.line(X_START_POS, TFTscreen.height()-valHeight(Y_AXIS_VAL_1)-1, TFTscreen.width()-1, TFTscreen.height()-valHeight(Y_AXIS_VAL_1)-1);
  TFTscreen.line(X_START_POS, TFTscreen.height()-valHeight(Y_AXIS_VAL_2)-1, TFTscreen.width()-1, TFTscreen.height()-valHeight(Y_AXIS_VAL_2)-1);
//  TFTscreen.line(X_START_POS, TFTscreen.height()-valHeight(Y_AXIS_VAL_3)-1, TFTscreen.width()-1, TFTscreen.height()-valHeight(Y_AXIS_VAL_3)-1);
}

void resetScreen()
{
  xPos=X_START_POS+1;
     // clear the screen with a pretty color
  TFTscreen.background(250, 16, 200);
   // write the static text to the screen
  // set the font color to white
  TFTscreen.stroke(255, 255, 255);
  // set the font size
  TFTscreen.setTextSize(2);
  // write the text to the top left corner of the screen
  TFTscreen.text("CPM\n",  TFTscreen.width()-35, 21);
  // set the font color to white
  TFTscreen.stroke(150, 150, 255);
  TFTscreen.text("muSv/h\n", TFTscreen.width()-72, 39);
  // set the fill color to grey
  TFTscreen.fill(255,255,224);  
  // draw a rectangle in the center of screen
  TFTscreen.rect(0, TFTscreen.height()*Y_GRAPH_START/100, TFTscreen.width()+1,TFTscreen.height()*Y_GRAPH_HEIGHT/100+1);    
  drawReferenceFrame();
}


int valHeight(int val)
{
  int drawHeight;
  if (val<Y_AXIS_VAL_1)
    drawHeight=map(val,0,Y_AXIS_VAL_1,0,(TFTscreen.height()*Y_GRAPH_HEIGHT/100-4)/N_Y_DIV);
  else if (val<Y_AXIS_VAL_2)
    drawHeight=(TFTscreen.height()*Y_GRAPH_HEIGHT/100-4)/N_Y_DIV+map(val,Y_AXIS_VAL_1,Y_AXIS_VAL_2,0,(TFTscreen.height()*Y_GRAPH_HEIGHT/100-4)/N_Y_DIV);
//  else if (val<Y_AXIS_VAL_3)
//    drawHeight=(TFTscreen.height()*Y_GRAPH_HEIGHT/100-4)/N_Y_DIV+map(val,Y_AXIS_VAL_2,Y_AXIS_VAL_3,0,(TFTscreen.height()*Y_GRAPH_HEIGHT/100-4)/N_Y_DIV);
  else  
    drawHeight=(TFTscreen.height()*Y_GRAPH_HEIGHT/100-4)*2/N_Y_DIV+map(val,Y_AXIS_VAL_2,Y_AXIS_VAL_MAX,0,(TFTscreen.height()*Y_GRAPH_HEIGHT/100-4)/N_Y_DIV);
 
  if (drawHeight> TFTscreen.height()*Y_GRAPH_HEIGHT/100-4) {
    drawHeight=TFTscreen.height()*Y_GRAPH_HEIGHT/100-4;
  }
  
  return drawHeight;
}
void welcomeScreen()
{  
  // clear the screen with a pretty color
  TFTscreen.background(250, 16, 200);
  // write the static text to the screen
  // set the font color to white
  TFTscreen.stroke(255, 255, 255);
  // set the font size
  TFTscreen.setTextSize(1);
  // write the text to the top left corner of the screen
//  TFTscreen.text("++Geiger++\n", 0, 0);
//  TFTscreen.text("++V0.1++\n", 0, 10);
  delay(2000);
}

void initFailed()
{
  TFTscreen.text("Init fale\n", 0, 0);
}

void initOK()
{
  TFTscreen.text("FW V0.2\n", 0, 0);
  TFTscreen.text("OK\n", 0, 10);
#ifdef ENABLE_SD_LOG
  TFTscreen.text("WRITE TO GEIGER.CSV\n", 0, 20);
#endif
   delay(3000);
}

void pulse() {
    ++pulses;
}

void drawGraphCPM(int r, int g, int b)
{
  // draw a line in a nice color
  TFTscreen.stroke(r,g,b);
  //  TFTscreen.line(xPos, TFTscreen.height()-drawHeight, xPos, TFTscreen.height());
  TFTscreen.line(xPos, TFTscreen.height()-valHeight(cpm)-1, xPos, TFTscreen.height()-1);
  
  // if the graph has reached the screen edge
  // erase the screen and start again
  if (xPos >= TFTscreen.width()-1) {
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
  TFTscreen.setTextSize(3);
  // write the text to the top left corner of the screen
  int  printVal = cpm; 
  if (printVal>Y_AXIS_VAL_MAX)
    printVal=Y_AXIS_VAL_MAX;
    
  String sensorVal = String(printVal);
  sensorVal.toCharArray(sensorPrintout, 6);

  TFTscreen.text(sensorPrintout,0,13);
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
  
  TFTscreen.text(sensorPrintout,0,39);
}
void logData()
{
  // make a string for assembling the data to log:
  String dataString = "";
  dataString += String(nmeasures);
  dataString += ",";
  dataString += String(cpm);
  dataString += ",";
  dataString += String(pulses);

  Serial.println(dataString);
  
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("GEIGER.CSV", FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("FILE_ERR");
  }
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
    
    logData();
    
    // reset the interrupt counter
    pulses = 0;
    // move the pointer to the next position in the ring
    pointer = (pointer + 1) % RING_SIZE;
    // calculate the uSv/h
    usvh = cpm * CPM_TO_USVH;

    drawGraphCPM(139,69,19);
    printCPM(255,255,255);
    printRadiation(150,150,150);
    
    drawReferenceFrame();
}



#ifdef EMULATE_GEIGER
void emulateGeigerCount()
{
   digitalWrite(GEIGER_EMULATOR, LOW);
   delayMicroseconds(700);
   digitalWrite(GEIGER_EMULATOR, HIGH);
}
#endif

void backLightSet()
{
  int buttonState = digitalRead(bckLitButton);
  // check if the pushbutton is pressed.
  // if it is, the buttonState is HIGH:
  
  if (buttonState == LOW  && ! bckLitIsPressed) {
#ifdef DEBUG    
    Serial.println("Button is pressed");
#endif
    bckLitIsPressed = true;
    bckLitVal++;
    bckLitVal=bckLitVal%3;
  }
  else if (buttonState == HIGH)
    bckLitIsPressed = false;
    
  analogWrite(LIT_PWM,254-(bckLitVal)*122);
  /*  
  // 3-state driven backlight settings, each button press cycle between them  
  switch (bckLitVal) {
    case 1:
      analogWrite(LIT_PWM,122);
      break;
    case 2:
      analogWrite(LIT_PWM,255);
      break;
    default: 
      analogWrite(LIT_PWM,0);
  }
  */
}
/*
void createLogFile()
{
  // re-open the file for reading:
  File myFile = SD.open("lastRun.txt");
  
//  if (myFile) {
//#ifdef DEBUG    
//    while (myFile.available()) {
//      Serial.write(myFile.read());
//    }
//#endif
//   myFile.close();
//  }
}
*/
////////////////////////////////////////////////////////
///
///    Now the real execution code
///
////////////////////////////////////////////////////////

bool setupStatus=false;

void setup() {
   
  // initialize the serial port
  Serial.begin(9600);
#ifdef DEBUG 
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("\nOK let's go");
#endif

 
  // initialize the display
  TFTscreen.begin();
  TFTscreen.setRotation(0);

  xPos=0;
  
  
  welcomeScreen();
  
  // try to access the SD card. If that fails (e.g.
  // no card present), the setup process will stop.
  //Serial.print(F("Initializing SD card..."));
  pinMode(10, OUTPUT);
#ifdef ENABLE_SD_LOG
  if (!SD.begin(sd_cs)) {
      //Serial.println(F("SD init failed!"));
      initFailed();
      return;
  }
#endif
  //Serial.println(F("OK!"));

  initOK();
  resetScreen();

#ifdef EMULATE_GEIGER 
  pinMode(GEIGER_EMULATOR, OUTPUT);
  digitalWrite(GEIGER_EMULATOR, HIGH); 
#endif  

  // allow pulse to trigger interrupt on falling
  attachInterrupt(GEIGER_INTERRUPT, pulse, FALLING);
  pinMode(LIT_PWM, OUTPUT);
  // initialize the pushbutton pin as an input:
  pinMode(bckLitButton, INPUT_PULLUP);

  setupStatus = true;
}


void loop() {
  if (!setupStatus)
    return;
    
  backLightSet();
    
#ifdef EMULATE_GEIGER  
    if (millis()>next_emulated_count)
    {
      next_emulated_count=millis()+PERIOD_LENGTH / GEIGER_EMULATOR_CPM;
      emulateGeigerCount();
    }
#endif

    if (millis() > next_update) {
        next_update = millis() + PERIOD_LENGTH / UPDATES_PER_PERIOD;
        update();
    } 
}

