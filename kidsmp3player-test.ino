
  /*
   * Problem: How to fill the Array for the number of Files, e.g.
   * numberOfFiles[] = { 3, 5, 2, 7, 3, 2, 1, 6, 7}
   * with mp3.queryFolderFiles() ??
   */

 /********************************************************************************/
/****************************** CONSTANTS ****************************************/
/********************************************************************************/

#include <MD_YX5300.h>
//  https://github.com/MajicDesigns/MD_YX5300/


// Connections for serial interface to the YX5300 module
const uint8_t ARDUINO_RX = 5;    // connect to TX of MP3 Player module
const uint8_t ARDUINO_TX = 6;    // connect to RX of MP3 Player module

// Constants for Buttons
#define PIN_KEY A3
#define BUTTON_TOLERANCE 25
#define LONG_KEY_PRESS_TIME_MS 2000L

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

/********************************************************************************/
/****************************** VARIABLES (May change) **************************/
/********************************************************************************/

/****** Folder Management *****/
int numberOfFolders = 9;
uint8_t currentFolder = 1; // start with folder 1
unsigned int currentFile = 1; // FIXME change Type?
unsigned int numberOfFiles[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0}; // for 9 Folders
int numberoffilesinfolder;
/******************************/

/****** Button Management *****/
int key = -1;
unsigned long keyPressTimeMs = 0;

unsigned long nowMs;

int keyPressRepeats = 0; //???

long buttonTimer = 0;
long longPressTime = 250;

boolean buttonActive = false;
boolean longPressActive = false;
// from https://www.instructables.com/id/Arduino-Dual-Function-Button-Long-PressShort-Press/

enum {
  MODE_NORMAL, MODE_SET_TIMER
} mode = MODE_NORMAL;
/******************************/

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
  int16_t curTrack;       // current track being played
  uint16_t volume;        // the current audio volume
} S;
/**************************/

/********************************************************************************/
/****************************** FUNCTIONS ******************************************/
/********************************************************************************/

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
/******************************/

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
/******************************/


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
    S.needUpdate = true;
     numberOfFiles[currentFolder-1] = S.numTracks; // #FIXME - Data type / Conversion Problem ??
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
/******************************/


void processPause(bool b)
// switches pause on if b == true; of if b == false
{
 
  if (b == true) {mp3.playPause();} else mp3.playStart();
}


void setVolume(int volume){
  // Max. Volume = mp3.volumeMax()
  mp3.volume(volume);
}



void increaseVolume();
void decreaseVolume();

/******************************/
/******************************/

/******************************/
/******************************/

/******************************/
/******************************/

/****** Handle Button Presses *****/
// Check for Key Press and Do Something
inline void handleKeyPress() {
  int keyCurrent = analogRead(PIN_KEY);
 

   if (keyCurrent <= 958) {
    int keyOld = key;
    if (keyCurrent > 933 - BUTTON_TOLERANCE) {
      key = 11;
      // Do Something
      Serial.print("key = 11 ");
      
    } else if (keyCurrent > 846 - BUTTON_TOLERANCE) {
      key = 9;
      // Do Something
      Serial.print("key = 9 ");
    } else if (keyCurrent > 760 - BUTTON_TOLERANCE) {
      key = 6;
      // Do Something
      Serial.print("key = 6 ");
    } else if (keyCurrent > 676 - BUTTON_TOLERANCE) {
      key = 3;
            Serial.print("key = 3 ");
            
            mp3.playSpecific(currentFolder, currentFile);
    } else if (keyCurrent > 590 - BUTTON_TOLERANCE) {
      key = 2;
            Serial.print("key = 2 ");
            
    } else if (keyCurrent > 504 - BUTTON_TOLERANCE) {
      key = 5;
            Serial.print("key = 5 ");
    } else if (keyCurrent > 414 - BUTTON_TOLERANCE) {
      key = 8;
            Serial.print("key = 8 ");
    } else if (keyCurrent > 321 - BUTTON_TOLERANCE) {
      key = 10;
            Serial.print("key = 10 ");
    } else if (keyCurrent > 222 - BUTTON_TOLERANCE) {
      key = 7;
            Serial.print("key = 7 ");
    } else if (keyCurrent > 115 - BUTTON_TOLERANCE) {
      key = 4;
            Serial.print("key = 4 ");
    } else if (keyCurrent > 0) {

      // key 1 pressed
      key = 1;
        Serial.print("key = 1"); 

        currentFolder = 1;
        mp3.playSpecific(currentFolder, currentFile);
    } 

     currentFolder = key;
     currentFile = 1;
     mp3.playSpecific(currentFolder, currentFile);
     
    if (keyOld != key) {
     keyPressTimeMs = nowMs;
    } 
  } else { 
    //  if no keys pressed
       
      }
}
/******************************/

/********************************************************************************/
/****************************** SETUP ******************************************/
/********************************************************************************/


void setup() {

#if DEBUG
  Serial.begin(9600);
#endif
  PRINTS("\n[MD_YX5300 Test]");

  // Initialize global libraries
  mp3.begin();
  mp3.setSynchronous(true);
  mp3.setCallback(cbResponse);
  S.initialising = initData(true);  // initialize data from MP3 device
  // Returns true if the initilisation must keep going, fails when completed.

  /*** buttons ***/
  pinMode(PIN_KEY, INPUT_PULLUP);
    
  Serial.print("Buttons ready");


  
}


  
/********************************************************************************/
/****************************** LOOP ******************************************/
/********************************************************************************/
void loop() {
  long int nowMs = millis();
  
  
  mp3.check();        // run the mp3 receiver   
  if (S.initialising && !S.waiting) S.initialising = initData();
  // initialize if waiting
  else {
    // check buttons, act on key press
    handleKeyPress(); 
  }
  
  
}
