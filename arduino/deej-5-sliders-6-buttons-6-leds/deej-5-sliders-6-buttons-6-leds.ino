#include <Mouse.h>

#include <Keyboard.h>
#include <KeyboardLayout.h>
#include <Keyboard_da_DK.h>
#include <Keyboard_de_DE.h>
#include <Keyboard_es_ES.h>
#include <Keyboard_fr_FR.h>
#include <Keyboard_it_IT.h>
#include <Keyboard_sv_SE.h>

//Change your keyboard layout in Keyboard.begin(), blank if american.

#include <AceButton.h>
using namespace ace_button;

//#define DEBUG_BUTTONS
//#define DEBUG_POTS

#define LINUX /* use Linux keyboard shortcuts */
//#define MACOS /* use MacOS keyboard shortcuts !NOT WORKING! */
//#define WINDOWS /* use Windows keyboard shortcuts */

/* LINUX:
Copy your config to your home dir: cp ~/go/pkg/mod/github.com/omriharel/deej@v0.9.10/config.yaml ~/
Link the deej binary to your home dir: ln -s ~/go/pkg/mod/github.com/omriharel/deej@v0.9.10/deej-release ~/deej
*/
#define CMD_DEEJ "./deej"
/* WINDOWS:
Add deej.exe to a folder that's in your systems path. Untested, let me know if it works.
*/
//#define CMD_DEEJ "deej.exe"

#define CMD_MEDIA_PLAYER "flatpak run com.plexamp.Plexamp" //command for your media pleyer of choice

const int NUM_SLIDERS = 5;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3, A6};

int analogSliderValues[NUM_SLIDERS];

const int NUM_BUTTONS = 6;
const int buttonInputs[NUM_BUTTONS] = {16, 8, 7, 15, 2, 0};
AceButton button1(buttonInputs[0]);
AceButton button2(buttonInputs[1]);
AceButton button3(buttonInputs[2]);
AceButton button4(buttonInputs[3]);
AceButton button5(buttonInputs[4]);
AceButton button6(buttonInputs[5]);

void handleEvent(AceButton*, uint8_t, uint8_t);

uint8_t layer = 0; //Macro keypad layer
bool mute = 0;
bool muteMic = 0;
bool mouseSpam = 0; //spam mouse clicks every in main loop if true
bool mouseHold = 0; // hold the mouse button down
#define KEYBOARD_HOLD 10 //how long to hold a key pressed before releasing
#define CLICK_COOLDOWN 25 //how long to dalay to make clicks register properly, in my case 25ms

const int NUM_LEDS = 6;
const int ledOutputs[NUM_LEDS] = {10, 9, 6, 14/* No PWN */, 3, 5};
#define LED_ON 0
#define LED_OFF 1
#define LED_LAYER_0 3
#define LED_LAYER_1 4
#define LED_LAYER_2 0
#define LED_LAYER_3 1
#define LED_MUTE 5
#define LED_MUTE_MIC 2

void sendKey (uint8_t key, uint8_t raw = 0) {
  Keyboard.press(key, raw);
  delay(KEYBOARD_HOLD);
  Keyboard.release(key, raw);
}

void setLeds (){
  digitalWrite(ledOutputs[LED_LAYER_0], (layer==0) ? LED_ON : LED_OFF);
  digitalWrite(ledOutputs[LED_LAYER_1], (layer==1) ? LED_ON : LED_OFF);
  digitalWrite(ledOutputs[LED_LAYER_2], (layer==2) ? LED_ON : LED_OFF);
  digitalWrite(ledOutputs[LED_LAYER_3], (layer==3) ? LED_ON : LED_OFF);
  digitalWrite(ledOutputs[LED_MUTE], (mute) ? LED_ON : LED_OFF);
  digitalWrite(ledOutputs[LED_MUTE_MIC], (muteMic) ? LED_ON : LED_OFF);
}

int log2lin(int l){
  const char exponent = 4; //try tweaking for your pots
  return (1023*pow(l/1023.,exponent));
}

int runCommand(const String& cmd){
         #ifdef LINUX
        Keyboard.press(KEY_LEFT_ALT);
        Keyboard.press(KEY_F2);
        delay(500);
        Keyboard.releaseAll();
        Keyboard.println(cmd);
        #endif
        
        #ifdef WINDOWS
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.write("r");
        delay(500);
        Keyboard.releaseAll();
        Keyboard.println(cmd);
        #endif

        #ifdef MACOS
        #warning "Building for MacOS!"
        /* MACOS
        I leave this for someone with some skin in the game.
        */
        #endif
}

void setup() { 
  Serial.begin(115200);

  while (! Serial) //Make sure serial is up on devices with native USB

    for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledOutputs[i],OUTPUT);
  }

  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(buttonInputs[i], INPUT);
  }

  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleButtonEvent);
  //buttonConfig->setFeature(ButtonConfig::kFeatureClick); //this click fires fast but if you double click you first get a click then a double click
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick); //this doesn't fire a click until the time for a double click has passed
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);
  buttonConfig->setRepeatPressInterval(10); //lets you spam buttons faster when using repeatpress

  Keyboard.begin(KeyboardLayout_sv_SE);
  Mouse.begin();
}

void loop() {
  updateSliderValues();

  analogSliderValues[0] = log2lin(analogSliderValues[0]); //convert log pots to something more linear
  analogSliderValues[1] = log2lin(analogSliderValues[1]); //convert log pots to something more linear

  #ifndef DEBUG_BUTTONS
    sendSliderValues(); // Actually send data (all the time)
  #endif
  #ifdef DEBUG_POTS
    printSliderValues(); // For debug
  #endif

  button1.check();
  button2.check();
  button3.check();
  button4.check();
  button5.check();
  button6.check();

  setLeds();

  if (mouseSpam) {
    Mouse.click();
    delay(CLICK_COOLDOWN);
  }
  
  delay(1);
}

void updateSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
     analogSliderValues[i] = analogRead(analogInputs[i]);
  }
}

void sendSliderValues() {
  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += String((int)analogSliderValues[i]);

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }
  }
  
  Serial.println(builtString);
}

void printSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    String printedString = String("Slider #") + String(i + 1) + String(": ") + String(analogSliderValues[i]) + String(" mV");
    Serial.write(printedString.c_str());

    if (i < NUM_SLIDERS - 1) {
      Serial.write(" | ");
    } else {
      Serial.write("\n");
    }
  }
}

void handleButtonEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  
  int pin = button->getPin();
  int btn = -1;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (buttonInputs[i] == pin) btn = i;

  }
  
  #ifdef DEBUG_BUTTONS
    // Print out a message for all events, for both buttons.
    Serial.print(F("button: "));
    Serial.print(btn);
    Serial.print("(");
    Serial.print(pin);
    Serial.print(F("); eventType: "));
    Serial.print(AceButton::eventName(eventType));
    Serial.print(F("; buttonState: "));
    Serial.println(buttonState);
  #endif
  
  //start with some things i want no matter what layer is selected
  if (btn == 3 && eventType == AceButton::kEventLongPressed) { //long press on button 4 too change layer
    layer++;
    if (layer > 3) layer = 0; //valid layers are 0-3
  } else if (btn == 5 && eventType == AceButton::kEventClicked) { //click 6 to mute output
    mute = !mute;
    sendKey(KEY_MUTE);
  } else if (btn == 5 && eventType == AceButton::kEventDoubleClicked) { //Double click 6 to toggle LED only
    mute = !mute;
  /*} else if (btn == 5 && eventType == AceButton::kEventRepeatPressed) { //Repeatedly fire when long pressing 6
  Mouse.click();*/
  } else if (btn == 5 && eventType == AceButton::kEventLongPressed) { //Start deej when long pressing 6
    runCommand(CMD_DEEJ);
  } else if (btn == 2 && (eventType == AceButton::kEventClicked || eventType == AceButton::kEventPressed || eventType == AceButton::kEventReleased)) { //click or hold 3 to mute mic
    muteMic = !muteMic;
    sendKey(KEY_MUTE_MIC);
  } else if (btn == 2 && eventType == AceButton::kEventDoubleClicked) { //Double click 3 to toggle LED only
    muteMic = !muteMic;
  }
  else switch (layer) { //otherwise it depends on what layer is selected
// -------- LAYER 0 -------- //
    case 0:
    switch (btn) {
      case 0: //bottom left
        switch (eventType) {
        case AceButton::kEventClicked:
        mouseSpam = 0;
        mouseHold = !mouseHold;
        mouseHold ? Mouse.press() : Mouse.release();
        break;
        case AceButton::kEventDoubleClicked:
        mouseHold = 0;
        mouseSpam = !mouseSpam;
        //actual clicks is made in main loop
        break;
        case AceButton::kEventRepeatPressed:
        Mouse.click();
        delay(CLICK_COOLDOWN);
        break;
      }
      break;
      case 1: //bottom middle
      switch (eventType) {
        case AceButton::kEventClicked:
        sendKey(KEY_LAUNCH5);
        break;
        case AceButton::kEventDoubleClicked:
        sendKey(KEY_LAUNCH6);
        break;
        case AceButton::kEventLongPressed:
        sendKey(KEY_LAUNCH7);
        break;
      }
      break;
      //case 2: //bottom right. fully reserved for mic mute functions
      case 3: //top left
      //long press reserved for switching layer
      switch (eventType) {
        case AceButton::kEventClicked:
        sendKey(KEY_LAUNCH8);
        break;
        case AceButton::kEventDoubleClicked:
          runCommand(CMD_MEDIA_PLAYER);
        break;
      }
      break;
      case 4: //top middle
      switch (eventType) {
        case AceButton::kEventClicked:
        sendKey(KEY_MEDIA_PLAYPAUSE);
        break;
        case AceButton::kEventDoubleClicked:
        sendKey(KEY_MEDIA_NEXTSONG);
        break;
        case AceButton::kEventLongPressed:
        sendKey(KEY_MEDIA_PREVIOUSSONG);
        break;
      }
      break;
      //case 5: //top right. reserved for mute functions, and starting deej
    }
    break;
// -------- LAYER 1 -------- //
    case 1:
    switch (btn) {
      case 0: //bottom left
      switch (eventType) {
        case AceButton::kEventClicked:
        sendKey(KEY_F21);
        break;
        case AceButton::kEventDoubleClicked:
        sendKey(KEY_F22);
        break;
        case AceButton::kEventLongPressed:
        sendKey(KEY_F23);
        break;
      }
      break;
      case 1: //bottom middle
      switch (eventType) {
        case AceButton::kEventClicked:
        sendKey(KEY_F24);
        break;
        case AceButton::kEventDoubleClicked:
        sendKey(KEY_EXECUTE);
        break;
        case AceButton::kEventLongPressed:
        sendKey(KEY_HELP);
        break;
      }
      break;
      //case 2: //bottom right. fully reserved for mic mute functions
      case 3: //top left
      //long press reserved for switching layer
      switch (eventType) {
        case AceButton::kEventClicked:
        sendKey(KEY_MENU);
        break;
        case AceButton::kEventDoubleClicked:
        sendKey(KEY_SELECT);
        break;
      }
      break;
      case 4: //top middle
      switch (eventType) {
        case AceButton::kEventClicked:
        sendKey(KEY_STOP);
        break;
        case AceButton::kEventDoubleClicked:
        sendKey(KEY_AGAIN);
        break;
        case AceButton::kEventLongPressed:
        sendKey(KEY_UNDO);
        break;
      }
      break;
      //case 5: //top right. reserved for mute functions, and starting deej
    }
    break;
// -------- LAYER 2 -------- //
    case 2:

    break;
// -------- LAYER 0 -------- //
    case 3:

    break;
    default:
    layer = 0; //something has gone wrong with layers, go back to 0
  }
}