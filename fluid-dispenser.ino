#include <NewPing.h>
#include <ShiftRegister74HC595.h>
#include <math.h>

// =================== IN/OUT NUMBERS ==================
// Buttons
#define TOGGLE_BUTTON_PIN 4 // IN
#define ACTION_BUTTON_PIN 3 // IN
#define BUZZER_PIN 5 // OUT
#define TRIGGER_PIN  7
#define ECHO_PIN     6
#define MAX_DISTANCE 450

#define SHIFTREG_SERIAL_DATA_PIN 8
#define SHIFTREG_CLOCK_PIN 10
#define SHIFTREG_LATCH_PIN 9

// Tasks names
#define MEASURE_DISTANCE_TASK 0
#define TOGGLE_BUTTON_TASK 1
#define ACTION_BUTTON_TASK 2

// =================== CONFIGURATION ===================
#define CUP_DETECTION_TIME 1000
#define BUTTONS_DEBOUNCE_TIME 200

// ===================== VARIABLES =====================
// Multitasking millis variables
unsigned long currentMillis = 0;
unsigned long prevMillis[2];
// Button states
boolean toggleButtonPressed = false;
boolean actionButtonPressed = false;
unsigned long buttonDebounceTime = 0;
// Distance measurement
int echoMeasurementsCount = 0;
unsigned long echoMeasurementTime = 0;
// Cup state
boolean isCup = false;
// Sound variables
boolean playingBeep = false;
unsigned int beepTime = 0;
boolean playingNegative = false;

// ===================== INSTANCES =====================
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
ShiftRegister74HC595 sr(1, SHIFTREG_SERIAL_DATA_PIN, SHIFTREG_CLOCK_PIN, SHIFTREG_LATCH_PIN);

// ======================= SETUP =======================
void setup() {
  pinMode(TOGGLE_BUTTON_PIN, INPUT);
  pinMode(ACTION_BUTTON_PIN, INPUT);
  pinMode(5, OUTPUT);
  Serial.begin(9600);
}

// ======================= LOOP ========================
void loop() { 
  currentMillis = millis();

  if (canPerformTask(MEASURE_DISTANCE_TASK, 100)) {
    performMeasureDistanceTask();
  }

  if (digitalRead(TOGGLE_BUTTON_PIN) == HIGH && !toggleButtonPressed && debounceButton()) {
    toggleButtonPressed = true;
  } else if (digitalRead(TOGGLE_BUTTON_PIN) == LOW && toggleButtonPressed) {
    perfomToggleButtonTask();
    toggleButtonPressed = false;
  }

  if (digitalRead(ACTION_BUTTON_PIN) == HIGH && !actionButtonPressed && debounceButton()) {
    actionButtonPressed = true;
  } else if (digitalRead(ACTION_BUTTON_PIN) == LOW && actionButtonPressed) {
    perfomActionButtonTask();
    actionButtonPressed = false;
  }

// Handle sounds
  handleSoundsTask();
}

// ======================= TASKS =======================
// ----------------------- T00: Measure distance task
void performMeasureDistanceTask() {
  if (sonar.ping_cm() != NO_ECHO) {
    echoMeasurementsCount++;
    echoMeasurementTime = currentMillis;
  } else {
    echoMeasurementsCount = 0;
  }
}

// ----------------------- T01: Toggle button task
void perfomToggleButtonTask() {
  Serial.println("Toggle button task");
  playBeep(200);
}

// ----------------------- T02: Action button task
void perfomActionButtonTask() {
  if (isCupPresent()) {
    Serial.println("OK");
    playBeep(500);
  } else {
    Serial.println("NO CUP");
    playNegative();
  }
}

// ----------------------- T05: Handle sounds task
void handleSoundsTask() {
  if (playingBeep) {
    handleBeepSound();  
  } else if (playingNegative) {
    handleNegativeSound();  
  }
}

// ===================== SOUNDS ========================
// ----------------------- S00: Beep
boolean beepSoundActive = false;
unsigned long beepSoundStartTime = 0;
void handleBeepSound() {
  if (!beepSoundActive) {
    beepSoundActive = true;
    beepSoundStartTime = currentMillis;
  } else {
    if (currentMillis - beepSoundStartTime >= beepTime) {
      beepSoundActive = false;
      playingBeep = false;
    }
  }
  digitalWrite(BUZZER_PIN, beepSoundActive);
}

// ----------------------- S01: Negative
boolean negativeSoundActive = false;
unsigned long negativeSoundStageTime = 0;
unsigned int negativeSoundStage = 0;
boolean negativeSoundStageSetted = false;
void handleNegativeSound() {
  if (!negativeSoundActive) {
    negativeSoundActive = true;
    negativeSoundStageTime = currentMillis;
  } else {
    if (negativeSoundStage == 0) {
      if (!negativeSoundStageSetted) {
        analogWrite(BUZZER_PIN, 220);
        negativeSoundStageSetted = true;
      }
      if (currentMillis - negativeSoundStageTime >= 200) {
        negativeSoundStageTime = currentMillis;
        negativeSoundStage = 1;
        negativeSoundStageSetted = false;
      }
    } else if (negativeSoundStage == 1) {
      if (!negativeSoundStageSetted) {
        analogWrite(BUZZER_PIN, 185);
        negativeSoundStageSetted = true;
      }
      if (currentMillis - negativeSoundStageTime >= 500) {
        negativeSoundStageTime = currentMillis;
        negativeSoundStage = 2;
        negativeSoundStageSetted = false;
      }
    } else {
      if (!negativeSoundStageSetted) {
        analogWrite(BUZZER_PIN, 0);
        negativeSoundActive = false;
        negativeSoundStage = 0;
        negativeSoundStageSetted = false;
//      Stop playing
        playingNegative = false;
      }
    }
  }
}

// ===================== FUNCTIONS =====================
// Alternative for delay() function
boolean canPerformTask(int index, unsigned long ms) {
// Check if task will should be performed
  if (currentMillis - prevMillis[index] >= ms) {
// Update last perform time
    prevMillis[index] = currentMillis;
    return true;
  }
  return false;
}

// Check if ultrasound sensor is covered
boolean isCupPresent() {
  return echoMeasurementsCount == 0 && currentMillis - echoMeasurementTime >= CUP_DETECTION_TIME;
}

// Debounce button function
boolean debounceButton() {
  return currentMillis - buttonDebounceTime > BUTTONS_DEBOUNCE_TIME;
}

// Play beep sound
void playBeep(unsigned int ms) {
  if (!playingBeep) {
    playingBeep = true;
    beepTime = ms;
  }
}
// Play negative sound
void playNegative() {
  if (!playingNegative) {
    playingNegative = true;
  }
}

void setFillingLevel(int level) {
  for (int i = 0; i < 8; i++) {
    sr.set(i, (level > i));
  }
}

void bip() {
  digitalWrite(5, HIGH);
  delay(50);
  digitalWrite(5, LOW);
}

void danger() {
  analogWrite(BUZZER_PIN, 255);
  delay(200);
  analogWrite(BUZZER_PIN, 200);
  delay(500);
  analogWrite(BUZZER_PIN, 0);
}
