#include <Arduino.h>
#include "MIDIUSB.h"
#include "Keyboard.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <ResponsiveAnalogRead.h>

#define SLIDER_WIDTH 15
#define SLIDER_HEIGHT 40
#define SLIDER_SPACING 30
#define SLIDER_X_OFFSET 30
#define SLIDER_Y_OFFSET 40
#define SLIDER_TEXT_OFFSET 5 // Offset for text below the slider

// Define pins
#define TFT_CS     10
#define TFT_RST    9
#define TFT_DC     8

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

int selectedChannel = 0;
int octave = 0;
int notesOctave = 0;

// LEDS
const int ledPins[] = {A2, A1, A0};
const int numLeds = sizeof(ledPins) / sizeof(ledPins[0]);
bool ledStates[numLeds] = {0, 0, 0}; // States of the LEDs

// MUX
// Outputs
int muxChannel1 = 2;
int muxChannel2 = 3;
int muxChannel3 = 4;
int muxChannel4 = 5;

// Inputs
int muxPotsInput = A3; // 8 channel analog
int muxSwitchesInput1 = 6; // 16 channel digital
int muxSwitchesInput2 = 7; // 8 channel digital

// MULTIPLEXER 1 BUTTONS - 16 CH
const int NUMBER_MUX_1_BUTTONS = 16;
bool muxButtons1CurrentState[NUMBER_MUX_1_BUTTONS] = {0};
bool muxButtons1PreviousState[NUMBER_MUX_1_BUTTONS] = {0};

unsigned long lastDebounceTimeMUX1[NUMBER_MUX_1_BUTTONS] = {0};
unsigned long debounceDelayMUX1 = 5;

// MULTIPLEXER 2 BUTTONS - 8 CH
const int NUMBER_MUX_2_BUTTONS = 7;
bool muxButtons2CurrentState[NUMBER_MUX_2_BUTTONS] = {0};
bool muxButtons2PreviousState[NUMBER_MUX_2_BUTTONS] = {0};

unsigned long lastDebounceTimeMUX2[NUMBER_MUX_2_BUTTONS] = {0};
unsigned long debounceDelayMUX2 = 5;

// MULTIPLEXER 3 POTS - 8 CH
const int NUMBER_MUX_3_POTS = 3;
int muxPots3PreviousState[NUMBER_MUX_3_POTS] = {0};
int muxPots3PreviousMidiState[NUMBER_MUX_3_POTS] = {0}; // To store previous MIDI states
int previousSliderValues[NUMBER_MUX_3_POTS] = {0};
bool previousSliderStates[NUMBER_MUX_3_POTS] = {0}; // Declare previousSliderStates

unsigned long lastDebounceTimeMUX3[NUMBER_MUX_3_POTS] = {0};
const unsigned long debounceDelayMUX3 = 5;
const int varThreshold = 10;

ResponsiveAnalogRead pots[NUMBER_MUX_3_POTS] = {
  ResponsiveAnalogRead(A3, true),
  ResponsiveAnalogRead(A3, true),
  ResponsiveAnalogRead(A3, true)
};

// Function to draw initial sliders
void initializeSliders() {
  int potValues[NUMBER_MUX_3_POTS];
  
  // Read initial pot values
  for (int i = 0; i < NUMBER_MUX_3_POTS; i++) {
    digitalWrite(muxChannel1, bitRead(i, 0));
    digitalWrite(muxChannel2, bitRead(i, 1));
    digitalWrite(muxChannel3, bitRead(i, 2));
    delay(1);  // Minimize this delay if response is too slow
    
    pots[i].update();
    int potValue = pots[i].getValue();
    potValues[i] = potValue;
    previousSliderValues[i] = potValue; // Set initial previous values
  }

  // Draw sliders with initial values
  for (int i = 0; i < NUMBER_MUX_3_POTS; i++) {
    int sliderX = SLIDER_X_OFFSET + i * SLIDER_SPACING;
    drawSlider(sliderX, SLIDER_Y_OFFSET, SLIDER_HEIGHT, 0, potValues[i], ledStates[i]);
  }
}

int currentScene = 1;
bool streamStarted = 0;
bool recordingStarted = 0;

void setup() {
  // Initialize serial communication for debugging (optional)
  Serial.begin(9600);

  // Configure multiplexer output pins
  pinMode(muxChannel1, OUTPUT);
  pinMode(muxChannel2, OUTPUT);
  pinMode(muxChannel3, OUTPUT);
  pinMode(muxChannel4, OUTPUT);
  digitalWrite(muxChannel1, LOW);
  digitalWrite(muxChannel2, LOW);
  digitalWrite(muxChannel3, LOW);
  digitalWrite(muxChannel4, LOW);

  // Set up multiplexer input pins
  pinMode(muxPotsInput, INPUT); // Analog input for potentiometers
  pinMode(muxSwitchesInput1, INPUT_PULLUP); // Digital input for the first set of switches
  pinMode(muxSwitchesInput2, INPUT_PULLUP); // Digital input for the second set of switches
  
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  tft.initR(INITR_144GREENTAB); // Initialize the display

  tft.fillScreen(ST77XX_BLACK);
   // Display a color gradient
  //displayColorGradient();

 
  updateStreamingStatus();
  clearRecordingDot();

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 25);
  tft.println("MIC Music SFX");

  // Draw initial scene text
  drawSceneText();

  // Initialize sliders
  initializeSliders();
}

void loop() {
  updateMUX1Buttons();
  updateMUX2Buttons();
  updateMUX3Pots();
}

void updateStreamingStatus() {
  
  tft.setTextSize(2);
  tft.fillRect(0, 5, 120, 20, ST77XX_BLACK); // Clear previous percentage
  
  if (streamStarted){
    tft.setCursor(12, 5);
    tft.setTextColor(ST77XX_GREEN);
    tft.println("Streaming");
  } else {
    tft.setCursor(27, 5);
    tft.setTextColor(ST77XX_RED);
    tft.println("Stopped");  
  }
  
}
void drawRecordingDot() {
  int dotX = 10;  // Beginning of the x axis
  int dotY = 30;  // Middle of the y axis
  int dotRadius = 5;  // Radius of the dot
  
  tft.fillCircle(dotX, dotY, dotRadius, ST77XX_RED);
}

void clearRecordingDot() {
  int dotX = 10;  // Beginning of the x axis
  int dotY = 30;  // Middle of the y axis
  int dotRadius = 5;  // Radius of the dot
  
  tft.fillCircle(dotX, dotY, dotRadius, ST77XX_BLACK);
}


void displayColorGradient() {
  for (uint16_t y = 0; y < tft.height(); y++) {
    for (uint16_t x = 0; x < tft.width(); x++) {
      uint16_t color = tft.color565(x * 255 / tft.width(), y * 255 / tft.height(), (x + y) * 255 / (tft.width() + tft.height()));
      tft.drawPixel(x, y, color);
    }
  }
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(5, 10);
  tft.println("Controller");
}

void updateMUX1Buttons() {
  for (int i = 0; i < NUMBER_MUX_1_BUTTONS; i++) {
    int A = bitRead(i, 0);
    int B = bitRead(i, 1);
    int C = bitRead(i, 2);
    int D = bitRead(i, 3);
    digitalWrite(muxChannel1, A);
    digitalWrite(muxChannel2, B);
    digitalWrite(muxChannel3, C);
    digitalWrite(muxChannel4, D);
    delay(1);
    muxButtons1CurrentState[i] = digitalRead(muxSwitchesInput1);

    if ((millis() - lastDebounceTimeMUX1[i]) > debounceDelayMUX1) {
      if (muxButtons1CurrentState[i] != muxButtons1PreviousState[i]) {
        lastDebounceTimeMUX1[i] = millis();

        if (muxButtons1CurrentState[i] == LOW) {
          pressButton(1, i, 0);
//          Serial.print("Mux 1 Button: ");
//          Serial.println(i);
        } else {
          pressButton(1, i, 1);
        }

        muxButtons1PreviousState[i] = muxButtons1CurrentState[i];
      }
    }
  }
}

void updateMUX2Buttons() {
  for (int i = 0; i < NUMBER_MUX_2_BUTTONS; i++) {
    int A = bitRead(i, 0);
    int B = bitRead(i, 1);
    int C = bitRead(i, 2);

    digitalWrite(muxChannel1, A);
    digitalWrite(muxChannel2, B);
    digitalWrite(muxChannel3, C);

    delay(1);
    muxButtons2CurrentState[i] = digitalRead(muxSwitchesInput2);

    if ((millis() - lastDebounceTimeMUX2[i]) > debounceDelayMUX2) {
      if (muxButtons2CurrentState[i] != muxButtons2PreviousState[i]) {
        lastDebounceTimeMUX2[i] = millis();

        if (muxButtons2CurrentState[i] == LOW) {
          pressButton(2, i, 0);
//          Serial.print("Mux 2 Button: ");
//          Serial.println(i);
        } else {
          pressButton(2, i, 1);
        }

        muxButtons2PreviousState[i] = muxButtons2CurrentState[i];
      }
    }
  }
}

void updateMUX3Pots() {
  int potValues[NUMBER_MUX_3_POTS];
  for (int i = 0; i < NUMBER_MUX_3_POTS; i++) {
    digitalWrite(muxChannel1, bitRead(i, 0));
    digitalWrite(muxChannel2, bitRead(i, 1));
    digitalWrite(muxChannel3, bitRead(i, 2));
    delay(1);  // Minimize this delay if response is too slow

    // Read the potentiometer value using ResponsiveAnalogRead
    pots[i].update();
    int potValue = pots[i].getValue();
    potValues[i] = potValue;
    int midiValue = map(potValue, 0, 1023, 0, 127); // Map to MIDI range

    if (abs(potValue - muxPots3PreviousState[i]) > varThreshold) {
      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTimeMUX3[i]) > debounceDelayMUX3) {
        lastDebounceTimeMUX3[i] = currentTime;

        if (midiValue != muxPots3PreviousMidiState[i]) {
          switch (i) {
            case 0:
              midiControlChange(selectedChannel, 101, midiValue);
              midiControlChange(selectedChannel, 102, midiValue);
              midiControlChange(selectedChannel, 103, midiValue);
              midiControlChange(selectedChannel, 104, midiValue);
              midiControlChange(selectedChannel, 105, midiValue);
              midiControlChange(selectedChannel, 106, midiValue);
              midiControlChange(selectedChannel, 107, midiValue);
              midiControlChange(selectedChannel, 108, midiValue);
              break;
            case 1:
              midiControlChange(selectedChannel, 109, midiValue);
              midiControlChange(selectedChannel, 110, midiValue);
              midiControlChange(selectedChannel, 111, midiValue);
              midiControlChange(selectedChannel, 112, midiValue);
              midiControlChange(selectedChannel, 113, midiValue);
              midiControlChange(selectedChannel, 114, midiValue);
              midiControlChange(selectedChannel, 115, midiValue);
              midiControlChange(selectedChannel, 116, midiValue);
              break;
            case 2:
              midiControlChange(selectedChannel, 117, midiValue);
              midiControlChange(selectedChannel, 118, midiValue);
              midiControlChange(selectedChannel, 119, midiValue);
              midiControlChange(selectedChannel, 120, midiValue);
              midiControlChange(selectedChannel, 121, midiValue);
              midiControlChange(selectedChannel, 122, midiValue);
              midiControlChange(selectedChannel, 123, midiValue);
              midiControlChange(selectedChannel, 124, midiValue);
              break;
          }

          muxPots3PreviousState[i] = potValue;
          muxPots3PreviousMidiState[i] = midiValue;
        }
      }
    }
  }

  updateSliders(potValues); // Update the slider display
}

void drawSlider(int x, int y, int height, int oldValue, int newValue, bool ledState) {
  int oldFilledHeight = map(oldValue, 0, 1023, 0, height);
  int newFilledHeight = map(newValue, 0, 1023, 0, height);
  uint16_t color = ledState ? ST77XX_RED : ST77XX_GREEN;

  // Clear the part that has decreased
  if (newFilledHeight < oldFilledHeight) {
    tft.fillRect(x + 1, y + height - oldFilledHeight, SLIDER_WIDTH - 2, oldFilledHeight - newFilledHeight, ST77XX_BLACK);
  }

  // Draw the part that has increased
  if (newFilledHeight > 0) {
    tft.fillRect(x + 1, y + height - newFilledHeight, SLIDER_WIDTH - 2, newFilledHeight, color);
  }

  // Redraw the outline
  tft.drawRect(x, y, SLIDER_WIDTH, height, ST77XX_WHITE);

  // Display the percentage below the slider
  int percentage = map(newValue, 0, 1023, 0, 100);
  tft.fillRect(x, y + height + SLIDER_TEXT_OFFSET, SLIDER_WIDTH + 10, 10, ST77XX_BLACK); // Clear previous percentage
  tft.setCursor(x, y + height + SLIDER_TEXT_OFFSET);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(percentage);
  tft.print("%");
}

void updateSliders(int values[]) {
  for (int i = 0; i < NUMBER_MUX_3_POTS; i++) {
    if (values[i] != previousSliderValues[i] || ledStates[i] != previousSliderStates[i]) {
      int sliderX = SLIDER_X_OFFSET + i * SLIDER_SPACING;
      drawSlider(sliderX, SLIDER_Y_OFFSET, SLIDER_HEIGHT, previousSliderValues[i], values[i], ledStates[i]);
      previousSliderValues[i] = values[i];
      previousSliderStates[i] = ledStates[i];
    }
  }
}

void pressButton(int muxNumber, int buttonNumber, int state) {
  int midiNote = 70; // starting midi note
  if (muxNumber == 1) {
    int n = midiNote + buttonNumber;
    if (state == 0) {
      midiControlChange(selectedChannel, n, 127);
      if (n == 70){
        streamStarted = 0;
        updateStreamingStatus();
      } else if (n == 71) {
        streamStarted = 1;
        updateStreamingStatus();
      } else if (n == 72) {
        recordingStarted = 0;
        clearRecordingDot();
      } else if (n == 73) {
        recordingStarted = 1;
        drawRecordingDot(); 
      } else if (n == 74){
        currentScene = 1;
      } else if (n == 75){
        currentScene = 2;
      } else if (n == 76){
        currentScene = 7;
      } else if (n == 77){
        currentScene = 8;
      } else if (n == 78){
        currentScene = 9;
      } else if (n == 79){
        currentScene = 10;
      } else if (n == 80){
        currentScene = 3;
      } else if (n == 81){
        currentScene = 4;
      } else if (n == 82){
        currentScene = 11;
      } else if (n == 83){
        currentScene = 12;
      } else if (n == 84){
        currentScene = 5;
      } else if (n == 85){
        currentScene = 6;
      }
      if (n >= 74 && n <= 85){
        drawSceneText();  
      }
    } else {
      midiControlChange(selectedChannel, n, 0);
    }
   
  } else if (muxNumber == 2) {
    int n = midiNote + buttonNumber + 16;
    if (buttonNumber < 4){
      
      if (state == 0) {
        midiControlChange(selectedChannel, n, 127);
      } else {
        midiControlChange(selectedChannel, n, 0);
      }
    } else {
      switch (buttonNumber) {
        case 4:
          if (state == 0) {
            ledStates[0] = !ledStates[0];
            digitalWrite(ledPins[0], ledStates[0]);
            updateSliders(previousSliderValues); // Redraw sliders with new LED state
            midiControlChange(selectedChannel, n, ledStates[0]);
          }
          break;
        case 5:
          if (state == 0) {
            ledStates[1] = !ledStates[1];
            digitalWrite(ledPins[1], ledStates[1]);
            updateSliders(previousSliderValues); // Redraw sliders with new LED state
            midiControlChange(selectedChannel, n, ledStates[1]);
          }
          break;
        case 6:
          if (state == 0) {
            ledStates[2] = !ledStates[2];
            digitalWrite(ledPins[2], ledStates[2]);
            updateSliders(previousSliderValues); // Redraw sliders with new LED state
            for (int i = 0; i < 8; i++) {
              midiControlChange(selectedChannel, n + i, ledStates[2]);
            }
            
          }
          break;
      }
    }
  }
}

void midiNoteOn(byte channel, unsigned short note) {
  midiEventPacket_t midiNoteOn = { 0x09, 0x90 | channel, note, 100 };
  MidiUSB.sendMIDI(midiNoteOn);
  MidiUSB.flush();  // Ensure the message is sent immediately
}

void midiNoteOff(byte channel, unsigned short note) {
  midiEventPacket_t midiNoteOff = { 0x08, 0x80 | channel, note, 0 };
  MidiUSB.sendMIDI(midiNoteOff);
  MidiUSB.flush();  // Ensure the message is sent immediately
}

void midiControlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = { 0x0B, 0xB0 | channel, control, value };
  MidiUSB.sendMIDI(event);
  MidiUSB.flush();  // Ensure the message is sent immediately
}


void drawSceneText() {
  // Clear previous scene text
  tft.fillRect(25, 100, 100, 20, ST77XX_BLACK);
  
  // Draw new scene text
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(25, 100);
  tft.print("Scene ");
  tft.print(currentScene);
}
