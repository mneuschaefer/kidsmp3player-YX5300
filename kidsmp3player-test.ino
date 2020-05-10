
  #include <MD_YX5300.h>
//  https://github.com/MajicDesigns/MD_YX5300/
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <EEPROM.h>


// Connections for serial interface to the YX5300 module
const uint8_t ARDUINO_RX = 5;    // connect to TX of MP3 Player module
const uint8_t ARDUINO_TX = 6;    // connect to RX of MP3 Player module

// Constants for Buttons
#define PIN_KEY A3
#define BUTTON_TOLERANCE 25
#define LONG_KEY_PRESS_TIME_MS 1000

#define PIN_VOLUME A5
#define PIN_VOLUME_INTERNAL A0


#define DEBUG 1 // enable/disable debug output

#ifdef DEBUG
#define PRINT(s,v)    { Serial.print(F(s)); Serial.print(v); }
#define PRINTX(s,v)   { Serial.print(F(s)); Serial.print(v, HEX); }
#define PRINTS(s)     { Serial.print(F(s)); }
#else
#define PRINT(s,v)
#define PRINTX(s,v)
#define PRINTS(s)
#endif

bool showFolderFiles = true;
long int lastCheck;

// Define global variables
MD_YX5300 mp3(ARDUINO_RX, ARDUINO_TX);

/************ Options **************************/
#define DEV_TF            0X02

#define EEPROM_CFG 1
#define EEPROM_FOLDER 2
#define EEPROM_TRACK 4



/******* Status LED ********/
#define LED_PIN 3
#define LED_DELAY 2000
bool ledIsOn = false;
int brightness = 0;    // how bright the LED is
int fadeAmount = 15;    // how many points to fade the LED by


unsigned long lastLedChange;
int ledBlinkPause = 200; //pause between blinks in MS

// https://www.baldengineer.com/fading-led-analogwrite-millis-example.html
// define directions for LED fade
#define UP 0
#define DOWN 1
 
// constants for min and max PWM
const int minPWM = 0;
const int maxPWM = 40;
 
// State Variable for Fade Direction
byte fadeDirection = UP;
 
// Global Fade Value
// but be bigger than byte and signed, for rollover
int fadeValue = 0;
 
// How smooth to fade?
byte fadeIncrement = 1;
 
// millis() timing Variable, just for fading
unsigned long previousFadeMillis;
 
// How fast to increment? (more is slower)
int fadeInterval = 30;

/********************************************************************************/
/****************************** VARIABLES (May change) **************************/
/********************************************************************************/

/****** Folder Management *****/
int numberOfFolders = 9;
uint8_t currentFolder = 1; // start with folder 1
unsigned int currentFile = 1; 
int numberOfFiles[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1}; // for 9 Folders
bool folderCheckComplete = false; // flag if all folders are checked in setup
bool foldersChecked[] = {false, false, false, false, false, false, false, false, false}; // for 9 folders

long int lastChecked;
int numberoffilesinfolder;
/******************************/

/****** Button Management *****/
int key = -1;
int button = -1; // replaces key #TODO
 int newButtonPressed;
 
unsigned long keyPressTimeMs = 0;
int debounceDelay = 800;

unsigned long nowMs;
unsigned long startMs;

int keyPressRepeats = 0; //???

long buttonTimer = 0;
long longPressTime = 250;

boolean buttonActive = false;
boolean longPressActive = false;
// from https://www.instructables.com/id/Arduino-Dual-Function-Button-Long-PressShort-Press/


enum {
  MODE_NORMAL, MODE_SET_TIMER
} mode = MODE_NORMAL;

bool b_playing = false;

// Timer
unsigned long lastButtonEvent;
unsigned long lastVolumeEvent;
unsigned long lastEepromEvent;
unsigned long lastStatusCheckEvent;
unsigned long lastKeyPress;
unsigned long lastLedEvent;
unsigned long ledOffTime;
int buttonToleranceMs = 800;


/*********Volume*****************/

float volFade = 1.0;
int vol = -1;
unsigned long volumeHandledLastMs = 0L;
#define VOLUME_CHECK_INTERVAL_MS 300L


/****** Status Information ****/
enum playMode_t { M_SEQ, M_SHUFFLE, M_LOOP, M_EJECTED };
enum playStatus_t { S_PAUSED, S_PLAYING, S_STOPPED };
enum encMode_t { E_VOLUME, E_TRACK };

struct    // contains all the running status information
{
  bool needUpdate;        // flag for display update required
  bool initialising;      // loading data from the device
  bool waiting;           // waiting initialisation response

  playMode_t playMode;    // playing mode 
  playStatus_t playStatus;// playing status
  encMode_t encMode;      // encoder mode
  uint16_t numTracks;     // number of tracks in this folder
  uint16_t curTrack;       // current track being played
  uint16_t volume;        // the current audio volume
} S;
/**************************/

/********** Initialize MP3 Data ************/
bool initData(bool reset = false)
// initialise data from the MP3 device. 
// This needs to be measured out as the data requests will generate 
// unsolicited messages to be handled in the callback and message 
// sequence is must be maintained with the synchronous message processing
// stream.
// Returns true if the initilisation must keep going, fals when completed.
{
  static uint8_t state = 0;
  bool b = true;

  if (reset)    // just rest the sequencing
  {
    state = 0;
  }
  else
  {
    switch (state)
    {
    case 0:   // set a default state in the devioce and then ask for first data
      mp3.playSpecific(currentFolder, currentFile);
      mp3.playPause();
      S.playMode = M_SEQ;
      S.playStatus = S_PAUSED;

      if (S.volume == 0)
        S.volume = (mp3.volumeMax() / 3) * 2;   // 2/3 of volume to start with
      mp3.volume(S.volume);

      // load number of files in the folder - needs wait for response
      mp3.queryFolderFiles(currentFolder);
      S.waiting = true;
      state++;
      break;

    case 1: // now load the track cplaying - needs wait for response
      mp3.queryFile();
      S.waiting = true;
      state++;
      break;

    default:
      // end of sequence handler
      state = 0;
      b = false;
      break;
    }
  }

  return(b);
}

/**********Select next song ***********/
void selectNextSong(int direction = 0)
// Pick the next song to play based on playing mode set.
// If direction  < 0 then select a 'previous' song, otherwise the 
// 'next' song is selected.
{
  switch (S.playMode)
  {
  case M_SHUFFLE:
    {
      uint16_t x = random(S.numTracks) + 1;
      mp3.playTrack(x);
      PRINT("\nPlay SHUFFLE ", x);
    }
    break;

  case M_LOOP:
      mp3.playTrack(S.curTrack);
      PRINTS("\nPlay LOOP");
      break;

  case M_SEQ:
      if (direction < 0) mp3.playPrev(); else  mp3.playNext();
      PRINTS("\nPlay SEQ");
      break;
  }
  mp3.queryFile();    // force index the update in callback
}

/************ Callback Response *********/
void cbResponse(const MD_YX5300::cbData *status)
// Callback function used to process device unsolicited messages
// or responses to datya requests
{
  PRINTS("\n");
  switch (status->code)
  {
  case MD_YX5300::STS_FILE_END:   // track has ended
    PRINTS("STS_FILE_END");
    selectNextSong();
    break;

  case MD_YX5300::STS_TF_INSERT:  // card has been inserted
    PRINTS("STS_TF_INSERT"); 
    S.initialising = initData(true);
    break;

  case MD_YX5300::STS_TF_REMOVE:  // card has been removed
    PRINTS("STS_TF_REMOVE"); 
    S.playMode = M_EJECTED;
    S.playStatus = S_STOPPED;
    S.needUpdate = true;
    break;

  case MD_YX5300::STS_PLAYING:   // current track index 
    PRINTS("STS_PLAYING");    
    S.curTrack = status->data;
    S.needUpdate = true;
    break;

  case MD_YX5300::STS_FLDR_FILES: // number of files in the folder
    PRINTS("STS_FLDR_FILES"); 
    S.numTracks = status->data;
    delay(300); // maybe remove #TODO

    // if you get some file number data, add it to numberOfFiles array
     numberOfFiles[currentFolder-1] = S.numTracks;
     
      // confirming that folder was queried
      if (numberOfFiles[currentFolder-1] > -1) {
        foldersChecked[currentFolder - 1] = true;
        //Serial.println("");
        //Serial.print("***** # in Folder +"); Serial.print(currentFolder); Serial.print(" is "); Serial.print(numberOfFiles[currentFolder-1]); Serial.println(" *****"); Serial.println("");
        //Serial.println("current folder has been checked");
        }


      /*
      if (!folderCheckComplete) {
        Serial.println(""); Serial.print("checking Folder Nr. "); Serial.println(currentFolder);
        if (foldersChecked[0] == true && foldersChecked[1]  == true && foldersChecked[2]  == true && 
            foldersChecked[3]  == true && foldersChecked[4]  == true && foldersChecked[5]  == true && 
            foldersChecked[6]  == true && foldersChecked[7]  == true && foldersChecked[8]  == true ) {
          folderCheckComplete = true;  Serial.println("Folder check complete");
          } else if (foldersChecked[currentFolder-1]){
            Serial.println(""); Serial.print("***** # in Folder +"); Serial.print(currentFolder); Serial.print(" is "); Serial.print(numberOfFiles[currentFolder-1]); Serial.println(" *****"); Serial.println("");
            Serial.println("current folder has been checked");
            }
      } */
      
      S.needUpdate = true;
    break;

  // unhandled cases
  case MD_YX5300::STS_VOLUME:     PRINTS("STS_VOLUME");     break;
  case MD_YX5300::STS_TOT_FILES:  PRINTS("STS_TOT_FILES");  break;
  case MD_YX5300::STS_ERR_FILE:   PRINTS("STS_ERR_FILE");   break;
  case MD_YX5300::STS_ACK_OK:     PRINTS("STS_ACK_OK");     break;
  case MD_YX5300::STS_INIT:       PRINTS("STS_INIT");       break;
  case MD_YX5300::STS_STATUS:     PRINTS("STS_STATUS");     break;
  case MD_YX5300::STS_EQUALIZER:  PRINTS("STS_EQUALIZER");  break;
  case MD_YX5300::STS_TOT_FLDR:   PRINTS("STS_TOT_FLDR");   break;
  default: PRINTX("STS_??? 0x", status->code); break;
  }

  PRINTX(", 0x", status->data);
  S.waiting = false;
}

void processPause(bool b)
// switches pause on if b == true; of if b == false
{
 
  if (b == true) {mp3.playPause();} else mp3.playStart();
}


void setVolume(int volume){
  // Max. Volume = mp3.volumeMax()
  mp3.volume(volume);
}



/****** Handle Button Presses *****/


inline void handleVolume() {
  //Serial.println("Volume = " + String(vol));
  if (nowMs > volumeHandledLastMs + VOLUME_CHECK_INTERVAL_MS) {
    volumeHandledLastMs = nowMs;

    int volCurrent = analogRead(PIN_VOLUME);
    // int volInternal = analogRead(PIN_VOLUME_INTERNAL);
    // removed vol internal
    //int volNew = (map(volCurrent, 0, 1023, 1, 31 - map(volInternal, 1023, 0, 1, 30))) ;
        int volNew = (map(volCurrent, 0, 1023, 1, 31)) ;

    if (volNew != vol) {
      vol = volNew;

 S.volume = (vol);
      mp3.volume(S.volume);
      
      setVolume(vol);
     // EEPROM.write(2, vol);
  }
  }
}

/************************* Button press ********************************/

// Prueft welcher Knopf gedrueckt wurde
// returns int for button, -1 if no button pressed
int checkButtonPressed() {
  int value = 1000;
  value = analogRead(PIN_KEY);
  if (debounce() == true) {
  if (value > 990) return -1;
  if (value > 933 - BUTTON_TOLERANCE) {
    Serial.println("Button 11 pressed");
    return 11;}
  if (value > 846 - BUTTON_TOLERANCE) {
    Serial.println("Button 9 pressed");
    return 9;}
  if (value > 760 - BUTTON_TOLERANCE) {
    Serial.println("Button 6 pressed");
    return 6;}
  if (value > 676 - BUTTON_TOLERANCE) {
    Serial.println("Button 3 pressed");
    return 3;}
  if (value > 590 - BUTTON_TOLERANCE) {
    Serial.println("Button 2 pressed");
    return 2;}
  if (value > 504 - BUTTON_TOLERANCE) {
    Serial.println("Button 5 pressed");
    return 5;}
  if (value > 414 - BUTTON_TOLERANCE) {
    Serial.println("Button 8 pressed");
    return 8;}
  if (value > 321 - BUTTON_TOLERANCE) {
    Serial.println("Button 10 pressed");
    return 10;}
  if (value > 222 - BUTTON_TOLERANCE) {
    Serial.println("Button 7 pressed");
    return 7;}
  if (value > 115 - BUTTON_TOLERANCE) {
    Serial.println("Button 4 pressed");
    return 4;}
  if (value > 0 ) {
    Serial.println("Button 1 pressed");
    return 1;}
  } else return -1;
}

void saveSongAndPositionInEeprom(){
 
    EEPROM.write(0, currentFolder);
    EEPROM.write(1, currentFile);
    EEPROM.write(2, vol);
 
 // EEPROMWriteLong(2, filePosition);
  lastEepromEvent = millis();
}

void loadPreviouslyPlayedSong(){
  int folderFromEeprom = EEPROM.read(0);
  int fileFromEeprom = EEPROM.read(1);
  int volFromEeprom = EEPROM.read(2);
  if (folderFromEeprom<=0 || fileFromEeprom<=0 || folderFromEeprom>10 || volFromEeprom>30){
    return;
  }
  currentFolder = folderFromEeprom;
  currentFile = fileFromEeprom;
  vol = volFromEeprom;
  //filePosition = EEPROMReadLong(2);
}

void doTheFadeUp(unsigned long thisMillis) {
   if (thisMillis - lastLedEvent >= fadeInterval) {
     fadeValue = fadeValue + fadeIncrement;  
      if (fadeValue >= maxPWM) {
        // At max, limit and change direction
        fadeValue = maxPWM;
        ledIsOn = true;
      }
      analogWrite(LED_PIN, fadeValue);  
 
    // reset millis for the next iteration (fade timer only)
    lastLedEvent = thisMillis;
   }
  }

  void doTheFadeDown(unsigned long thisMillis) {
   if (thisMillis - lastLedEvent >= fadeInterval) {
       fadeValue = fadeValue - fadeIncrement;
      if (fadeValue <= minPWM) {
        
        // At min, limit and change direction
        fadeValue = minPWM;
        //fadeDirection = UP;
        ledIsOn = false;
      }
      analogWrite(LED_PIN, fadeValue);  
 
    // reset millis for the next iteration (fade timer only)
    lastLedEvent = thisMillis;
  }
  }


// check buttons, returns true if button was pressed
boolean checkAndSetButtonPressed() { //#change #here
  
  // if no button was pressed: false
  if (newButtonPressed == -1 ){
    return false;
  }
  
  // if button = currentFolder: continue to next song
  if (currentFolder == newButtonPressed) {
    // next song
    playNextSong();
  }
  
  // if Button = 10: play pause or continue
  if (newButtonPressed == 10){
    pauseUnpause();
  }

  // if button = 11: reset currentFile to 1
   if (newButtonPressed == 11){
    currentFile = 1;
    b_playing = true; mp3.playSpecific(currentFolder, currentFile);
    // 
    return true;
   }
  button = newButtonPressed;
  currentFolder = newButtonPressed;
  currentFile = 1;
  lastButtonEvent = nowMs;

  // saves current song position in eeprom
  saveSongAndPositionInEeprom();
  
  return true;
}

void playNextSong(){
   Serial.println("next Track"); delay(50);
        currentFile++; 
        if (currentFile > numberOfFiles[currentFolder-1]) {currentFile = 1;}
         b_playing = true; mp3.playSpecific(currentFolder, currentFile);
  }

bool debounce(){
  if (millis() - lastButtonEvent < debounceDelay) {
    lastButtonEvent = nowMs;
    return true;
    } else return false;
  }
  
void pauseUnpause(){
//return nextPreviousSong(newButtonPressed); //change
      // mp3.playPause(); #CHECK #TODO
      if (b_playing == false) {
      mp3.playStart();
      b_playing = true;
      } else {
        mp3.playPause(); 
        S.playStatus = S_PAUSED; 
        b_playing = false;
        } 
        //delay(50);
  
}


void controlLed(){
  if (!b_playing) {analogWrite(LED_PIN, maxPWM);} else {
  
    if ((millis() - lastLedChange) > 0 && !ledIsOn &&
    ((millis() - ledOffTime) > ledBlinkPause)) {
      // led stays off for 1 secons
      lastLedChange = millis();
      doTheFadeUp(nowMs);
       }
    if ((millis() - lastLedChange) > 0 && ledIsOn) {
      lastLedChange = millis();
      doTheFadeDown(nowMs);
      if (!ledIsOn){ledOffTime = nowMs;}  
      }
    }  
}


void setup(){

#if DEBUG
  Serial.begin(9600);
#endif
  PRINTS("\n[MD_YX5300 Test]");


  // Volume (Potis)
  pinMode(PIN_VOLUME, INPUT);
  pinMode(PIN_VOLUME_INTERNAL, INPUT);

  // Initialize global libraries
  mp3.begin();
    
  mp3.setSynchronous(true);
  mp3.setCallback(cbResponse);
  //S.initialising = initData(true);  // initialize data from MP3 device
  // Returns true if the initilisation must keep going, fails when completed.
//S.playMode = M_LOOP;


 pinMode(LED_PIN, fadeValue);
  
 /** buttons ***/
  pinMode(PIN_KEY, INPUT_PULLUP);
   
    
  Serial.print("Buttons ready");
long int lastChecked = millis() - 50;

// load previous song from eeprom
loadPreviouslyPlayedSong();
 
   nowMs = millis();
   lastLedEvent = nowMs;
   ledOffTime = nowMs;
     lastKeyPress= nowMs;
   startMs = nowMs;
  
}

void loop() {
  
  nowMs = millis();
  mp3.check();        // run the mp3 receiver  
  handleVolume(); 

  if (S.initialising && !S.waiting) {S.initialising = initData();}
  // initialize if waiting
  
  // every  300Ms, queryFolderFiles for the current folder
  // Problem: checks only current folder, creates lag when choosing new folder #FIXME
  if ((nowMs - lastChecked) > 300){ 
    if(numberOfFiles[currentFolder-1] < 0) {
      mp3.queryFolderFiles(currentFolder);
      delay(200);
      }
    lastChecked = nowMs;
  }

  //check for new button pressed
  // (only updates if debounce() == true // debounceDelay
  newButtonPressed = checkButtonPressed();

   // once the folders are checked, react to buttons
  if(foldersChecked) checkAndSetButtonPressed();
  
  // update LED (blinks when playing, pulses when not)
  controlLed();

}
  
