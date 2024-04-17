#include <MIDIUSB.h>

// For MIDI support you nedd an Arduino or compatible with native USB
// Known to work on AtMega32u4 like Pro Micro and Leonardo
const int MIDI_CHANNEL = 0;

const int NUM_SLIDERS = 5;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3, A6}; //A4 and A5 isn't available on some devices, using A6 instead

int analogSliderValues[NUM_SLIDERS];

void setup() { 
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  Serial.begin(9600);
}

void loop() {
  updateSliderValues();
  sendSliderValuesMIDI(); // Actually send data (all the time)
  printSliderValues(); // For debug
  delay(30);
}

void updateSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
     analogSliderValues[i] = analogRead(analogInputs[i]);
  }
}

void sendSliderValuesMIDI() {
  // Send slider values over MIDI.
  // Byte 0 is event type, in this case 0x09 for note on.
  // Byte 1 is event type in upper nibble, and MIDI channel on lower nibble.
  // Byte 2 is note number, slider in our case, 7 bit value.
  // Byte 3 is velocity, the value of the slider for us, also 7 bits.
  uint8_t data[NUM_SLIDERS * 4];
  for (int i = 0; i < NUM_SLIDERS; i++) {
    int offset = i*4; // we need an offset for each slider in the data buffer
    int value = analogSliderValues[i] >> 3; // bit shift 3 steps to right to convert 10 bit samples to MIDI friendly 7 bits
    data[offset+0] = 0x09;
    data[offset+1] = 0x90 | MIDI_CHANNEL;
    data[offset+2] = i;
    data[offset+3] = value & 0x7f;
  }
  MidiUSB.write(data, NUM_SLIDERS * 4);
  MidiUSB.flush();
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
