#include <NewPing.h>
#include <ShiftRegister74HC595.h>
#include <math.h>

// =================== IN/OUT NUMBERS ==================
#define TRIGGER_PIN  7
#define ECHO_PIN     6
#define MAX_DISTANCE 450

#define SHIFTREG_SERIAL_DATA_PIN 8
#define SHIFTREG_CLOCK_PIN 10
#define SHIFTREG_LATCH_PIN 9

// Tasks names
#define MEASURE_DISTANCE_TASK 0
#define TOGGLE_BUTTON_TASK 1

// ===================== VARIABLES =====================
// Multitasking millis variables
unsigned long currentMillis = 0;
unsigned long prevMillis[2];

// ===================== INSTANCES =====================
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);
ShiftRegister74HC595 sr(1, SHIFTREG_SERIAL_DATA_PIN, SHIFTREG_CLOCK_PIN, SHIFTREG_LATCH_PIN);

// ======================= SETUP =======================
void setup() {
  pinMode(4, INPUT);
  pinMode(5, OUTPUT);
  Serial.begin(9600);
}

// ======================= LOOP ========================
void loop() { 
  currentMillis = millis();

  if (canPerformTask(MEASURE_DISTANCE_TASK, 1000)) {
    performMeasureDistanceTask();
  }

  if (canPerformTask(TOGGLE_BUTTON_TASK, 100)) {
    if (digitalRead(4) == HIGH && !toggleButtonPressed) {
      toggleButtonPressed = true;
    } else if (digitalRead(4) == LOW && toggleButtonPressed) {
      perfomToggleButtonTask();
      toggleButtonPressed = false;
    }
  }

// ======================= TASKS =======================
// ----------------------- T01: Measure distance task
void performMeasureDistanceTask() {
  int distance = sonar.ping_cm();
  Serial.println(distance);
}

// ----------------------- T02: Toggle button task
boolean toggleButtonPressed = false;
void perfomToggleButtonTask() {

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
  for (int i = 0; i < 10; i++) {
    digitalWrite(5, HIGH);
    delay(100);
    digitalWrite(5, LOW);
    delay(50);
  }
}
