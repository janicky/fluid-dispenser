#include <NewPing.h>
#include <ShiftRegister74HC595.h>
#include <LiquidCrystal_I2C.h>
#include <DS18B20.h>
#include <math.h>

// =================== IN/OUT NUMBERS ==================
// Buttons
#define TOGGLE_BUTTON_PIN 4 // IN
#define ACTION_BUTTON_PIN 3 // IN
#define BUZZER_PIN 5 // OUT
#define TRIGGER_PIN  7
#define ECHO_PIN     6
#define WEIGHT_SENSOR_PIN A2 // ANALOG IN
#define TEMPERATURE_SENSOR_PIN 2 // IN
#define PUMP_PIN 11 // OUT
#define DIODE_PIN 12 // OUT
#define MAX_DISTANCE 450

#define SHIFTREG_SERIAL_DATA_PIN 8
#define SHIFTREG_CLOCK_PIN 10
#define SHIFTREG_LATCH_PIN 9

// Tasks names
#define MEASURE_DISTANCE_TASK 0
#define TOGGLE_BUTTON_TASK 1
#define ACTION_BUTTON_TASK 2
#define EXPIRE_TOGGLE_TASK 3
#define EXPIRE_NOVESSEL_TASK 4
#define MEASURE_WEIGHT_TASK 5
#define MEASURE_TEMPERATURE_TASK 6
#define PUMP_TASK 7
#define UPDATE_DIODE_TASK 8
#define EXPIRE_DONE_TASK 9

// =================== CONFIGURATION ===================
#define CUP_DETECTION_TIME 1000
#define BUTTONS_DEBOUNCE_TIME 200
#define TOGGLE_EXPIRATION_TIME 4000
#define NOVESSEL_EXPIRATION_TIME 6000
#define DONE_EXPIRATION_TIME 2000
#define DEFAULT_BUZZER_TONE 255
#define DEFAULT_DIODE_BLINK_TIME 500
#define TIME_PER_ML 48
const int VOLUMES[] = { 40, 100, 150, 200, 250, 330 };

// ===================== VARIABLES =====================
// Multitasking millis variables
unsigned long currentMillis = 0;
unsigned long prevMillis[10];
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
unsigned int beepTone = 255;
boolean playingNegative = false;
// Toggle button configuration
boolean isToggleActivated = false;
unsigned int toggleStage = 0;
// Done screen state
boolean isDoneScreenActive = false;
// Vessel detection
boolean noVesselActivated = false;
boolean isVesselAlreadyFilled = false;
// Weight measure
int weight = 0;
// Temperature measure
float temperature = 0.0;
// Screen variables
boolean isHomeScreenActive = true;
// Pump state
boolean isPumpActive = false;
unsigned long pumpStartTime = 0;
// Diode
boolean isDiodeActive = false;
boolean isDiodeBlinking = false;
int diodeBlinkTime = DEFAULT_DIODE_BLINK_TIME;

// ===================== INSTANCES =====================
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
ShiftRegister74HC595 sr(2, SHIFTREG_SERIAL_DATA_PIN, SHIFTREG_CLOCK_PIN, SHIFTREG_LATCH_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DS18B20 ds(TEMPERATURE_SENSOR_PIN);

// ======================= SETUP =======================
void setup() {
  pinMode(TOGGLE_BUTTON_PIN, INPUT);
  pinMode(ACTION_BUTTON_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(DIODE_PIN, OUTPUT);
  
  Serial.begin(9600);

  

  lcd.init();
  lcd.backlight();
  displayHomeScreen();
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
  } else if (digitalRead(ACTION_BUTTON_PIN) == LOW && actionButtonPressed && !isPumpActive) {
    perfomActionButtonTask();
    actionButtonPressed = false;
  }

  if (isToggleActivated && canPerformTask(EXPIRE_TOGGLE_TASK, TOGGLE_EXPIRATION_TIME)) {
    performExpireToggleTask();
  }

  if (noVesselActivated && canPerformTask(EXPIRE_NOVESSEL_TASK, NOVESSEL_EXPIRATION_TIME)) {
    performExpireNoVesselTask();
  }

  if (isDoneScreenActive && canPerformTask(EXPIRE_DONE_TASK, DONE_EXPIRATION_TIME)) {
    performExpireDoneTask();
  }

  if (canPerformTask(MEASURE_WEIGHT_TASK, 100)) {
    performMeasureWeightTask();
  }

  if (canPerformTask(MEASURE_TEMPERATURE_TASK, 20000)) {
    performMeasureTemperatureTask();
  }

  if (isPumpActive && canPerformTask(PUMP_TASK, 1)) {
    performPumpTask();
  }

  if (!isDiodeBlinking && canPerformTask(UPDATE_DIODE_TASK, 50)) {
    performUpdateDiodeTask();
  }

// Handle sounds
  handleSoundsTask();

// Handle diodes
  handleDiodesTask();

// Set pump state
  digitalWrite(PUMP_PIN, isPumpActive);
}

// ======================= TASKS =======================
// ----------------------- T00: Measure distance task
void performMeasureDistanceTask() {
  int distance = sonar.ping_cm();
  if (distance > 30) return;
  
  if (distance != NO_ECHO) {
    echoMeasurementsCount++;
    echoMeasurementTime = currentMillis;
  } else {
    echoMeasurementsCount = 0;
  }
}

// ----------------------- T01: Toggle button task
void perfomToggleButtonTask() {
  if (isPumpActive) {
    return;
  }
  if (isToggleActivated) {
    toggleStage = (toggleStage + 1) % (sizeof(VOLUMES) / sizeof(int));
    playBeep(200);
  }

  displayToggleScreen();
  isToggleActivated = true;
  updateTaskTime(EXPIRE_TOGGLE_TASK);
}

// ----------------------- T02: Action button task
void perfomActionButtonTask() {
  if (isCupPresent() && !isVesselAlreadyFilled) {
    pumpStartTime = currentMillis;
    isPumpActive = true;
    isDiodeBlinking = true;
    isToggleActivated = false;
    displayActionScreen();
    playBeep(1000);
  } else {
    displayNoVesselScreen();
    noVesselActivated = true;
    updateTaskTime(EXPIRE_NOVESSEL_TASK);
    playNegative();
  }
}

// ----------------------- T03: Expire toggle task
void performExpireToggleTask() {
  isToggleActivated = false;
  displayHomeScreen();
  beepTone = 100;
  playBeep(100);
}

// ----------------------- T04: Expire no vessel task
void performExpireNoVesselTask() {
  noVesselActivated = false;
  displayHomeScreen();
  beepTone = 100;
  playBeep(100);
}

// ----------------------- T05: Measure weight task
void performMeasureWeightTask() {
  weight = analogRead(WEIGHT_SENSOR_PIN);
  int level = map(weight, 820, 900, 0, 10);
  setFillingLevel(level);
}

// ----------------------- T06: Measure temperature task
void performMeasureTemperatureTask() {
  if (isHomeScreenActive) {
    temperature = ds.getTempC();
    displayHomeScreen();
  }
}

// ----------------------- T07: Pump task
void performPumpTask() {
  if (!isCupPresent()) {
    isVesselAlreadyFilled = false;
    isPumpActive = false;
    displayNoVesselScreen();
    noVesselActivated = true;
    updateTaskTime(EXPIRE_NOVESSEL_TASK);
    playNegative();
    isDiodeBlinking = false;
  }
  if (currentMillis - pumpStartTime >= getPourTime()) {
    displayDoneScreen();
    isVesselAlreadyFilled = true;
    isPumpActive = false;
    isDiodeActive = true;
    isDiodeBlinking = false;
    playBeep(500);
  }
}

// ----------------------- T08: Update diode task
void performUpdateDiodeTask() {
  if (isCupPresent() xor isDiodeActive) {
    beepTone = 50;
    playBeep(50);
  }
  isDiodeActive = isCupPresent();
  if (!isCupPresent()) {
    isVesselAlreadyFilled = false;
  }
}

// ----------------------- T09: Expire done task
void performExpireDoneTask() {
  isDoneScreenActive = false;
  displayHomeScreen();
  beepTone = 100;
  playBeep(100);
}



// ----------------------- THS: Handle sounds task
void handleSoundsTask() {
  if (playingBeep) {
    handleBeepSound();  
  } else if (playingNegative) {
    handleNegativeSound();  
  }
}

// ----------------------- THD: Handle diodes task
unsigned long lastBlinkTime = 0;
boolean diodeState = false;

void handleDiodesTask() {
  if (!isDiodeBlinking) {
    // Set diode state
    digitalWrite(DIODE_PIN, isDiodeActive);
  } else {
    if (currentMillis - lastBlinkTime >= diodeBlinkTime) {
      lastBlinkTime = currentMillis;
      diodeState = !diodeState;
      digitalWrite(DIODE_PIN, diodeState);
    }
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
      beepTone = DEFAULT_BUZZER_TONE;
    }
  }
  int buzzerValue = beepSoundActive ? beepTone : 0;
  analogWrite(BUZZER_PIN, buzzerValue);
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
        analogWrite(BUZZER_PIN, 0);
        negativeSoundStageSetted = true;
      }
      if (currentMillis - negativeSoundStageTime >= 50) {
        negativeSoundStageTime = currentMillis;
        negativeSoundStage = 2;
        negativeSoundStageSetted = false;
      }
    } else if (negativeSoundStage == 2) {
      if (!negativeSoundStageSetted) {
        analogWrite(BUZZER_PIN, 185);
        negativeSoundStageSetted = true;
      }
      if (currentMillis - negativeSoundStageTime >= 500) {
        negativeSoundStageTime = currentMillis;
        negativeSoundStage = 3;
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

// ===================== SCREENS ====================
// ----------------------- SC00: Home
void displayHomeScreen() {
  isHomeScreenActive = true;
  lcd.clear();
  lcd.print("TEMP:     " + String(temperature, 1) + "C");
  lcd.setCursor(0, 1);
  lcd.print("SELECTED: " + volumeText(VOLUMES[toggleStage]));
}

// ----------------------- SC01: Toggle
void displayToggleScreen() {
  isHomeScreenActive = false;
  lcd.clear();
  lcd.print("SELECTED: ");
  lcd.print(volumeText(VOLUMES[toggleStage]));
  lcd.setCursor(0, 1);
  lcd.print("pour - press red");
}

// ----------------------- SC02: Action
void displayActionScreen() {
  isHomeScreenActive = false;
  lcd.clear();
  lcd.print("PLEASE WAIT");
  lcd.setCursor(0, 1);
  lcd.print("POURING... ");
  lcd.print(volumeText(VOLUMES[toggleStage]));
}
// ----------------------- SC03: No vessel
void displayNoVesselScreen() {
  isHomeScreenActive = false;
  lcd.clear();
  lcd.print("NO VESSEL DETECT");
  lcd.setCursor(0, 1);
  lcd.print("Please try again");
}
// ----------------------- SC04: Done
void displayDoneScreen() {
  isHomeScreenActive = false;
  updateTaskTime(EXPIRE_DONE_TASK);
  isDoneScreenActive = true;
  lcd.clear();
  lcd.print("DONE!");
}

// ===================== FUNCTIONS =====================
// Alternative for delay() function
boolean canPerformTask(int index, unsigned long ms) {
// Check if task will should be performed
  if (abs(currentMillis - prevMillis[index]) >= ms) {
// Update last perform time
    updateTaskTime(index);
    return true;
  }
  return false;
}

// Update prev time
void updateTaskTime(int index) {
  prevMillis[index] = currentMillis;
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
  for (int i = 0; i < 10; i++) {
    sr.set(i, (level > i));
  }
}

String volumeText(int volume) {
  return String(volume, 10) + "ml";
}
