const int NUM_SLIDERS = 5;
const int analogInputs[NUM_SLIDERS] = {A0, A1, A2, A3, A4};

int analogSliderValues[NUM_SLIDERS];

unsigned long jiggleLast; //Last time a jiggle happened
const int jiggleDelay = 1000; //how many ms to wait between jiggles
const int jiggleStep = 36; //depends on noise reduction in config, my math says 16, 26, or 36

void setup() { 
  for (int i = 0; i < NUM_SLIDERS; i++) {
    pinMode(analogInputs[i], INPUT);
  }

  Serial.begin(9600);
}

void loop() {
  updateSliderValues();

  int jiggleSliders[2] = {1,2}; //sliders you want to jiggle
  jiggler(jiggleSliders, sizeof(jiggleSliders));

  sendSliderValues(); // Actually send data (all the time)
  // printSliderValues(); // For debug
  delay(10);
}

void jiggler(int sliders[], int count) { //jiggle a slider every once in a while
  if (millis() > jiggleLast + jiggleDelay){
    jiggleLast = millis();
    for (int i = 0; i < count; i++)
      analogSliderValues[sliders[i]] += analogSliderValues[sliders[i]] > 100 ? -jiggleStep : jiggleStep;
  }
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
