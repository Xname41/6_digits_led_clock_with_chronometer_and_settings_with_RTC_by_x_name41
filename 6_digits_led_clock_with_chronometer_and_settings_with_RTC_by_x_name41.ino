#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

// --- Настройки време показване ---
const byte NUM_DIGITS = 6;
const float refreshRateHz = 50.0;
const unsigned long DISPLAY_TIME_MS = 1000.0 / (refreshRateHz * NUM_DIGITS);


const byte segmentPins[7] = {10, 8, 7, 9, 2, 6, 3};
const byte digitPins[6] = {27, 26, 25, 24, 23, 22};

const byte digitToSegment[10][7] = {
  {LOW, LOW, LOW, LOW, LOW, LOW, HIGH},
  {HIGH, LOW, LOW, HIGH, HIGH, HIGH, HIGH},
  {LOW, LOW, HIGH, LOW, LOW, HIGH, LOW},
  {LOW, LOW, LOW, LOW, HIGH, HIGH, LOW},
  {HIGH, LOW, LOW, HIGH, HIGH, LOW, LOW},
  {LOW, HIGH, LOW, LOW, HIGH, LOW, LOW},
  {LOW, HIGH, LOW, LOW, LOW, LOW, LOW},
  {LOW, LOW, LOW, HIGH, HIGH, HIGH, HIGH},
  {LOW, LOW, LOW, LOW, LOW, LOW, LOW},
  {LOW, LOW, LOW, LOW, HIGH, LOW, LOW}
};

// Бутони
const byte setButtonPin = 12;
const byte plusButtonPin = 13;
const byte minusButtonPin = 14;

// Часови данни
byte hours = 12, minutes = 0, seconds = 0;

enum State { NORMAL, SET_HOURS, SET_MINUTES, SET_SECONDS, STOPWATCH };
State mode = NORMAL;

unsigned long blinkTimer = 0;
byte currentDigit = 0;
bool blinkOn = true;

bool lastSetState = HIGH;
bool lastPlusState = HIGH;
bool lastMinusState = HIGH;

unsigned long setPressTime = 0;
bool setHeld = false;

unsigned long plusPressTime = 0;
bool plusHeld = false;

unsigned long minusPressTime = 0;
bool minusHeld = false;

// Хронометър
bool stopwatchRunning = false;
unsigned long stopwatchStartTime = 0;
unsigned long stopwatchElapsed = 0;

void clearSegments() {
  for (byte i = 0; i < 7; i++) digitalWrite(segmentPins[i], HIGH);
}
void clearDigits() {
  for (byte i = 0; i < 6; i++) digitalWrite(digitPins[i], LOW);
}

void setup() {
  Wire.begin();
  rtc.begin();

  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  for (byte i = 0; i < 7; i++) pinMode(segmentPins[i], OUTPUT);
  for (byte i = 0; i < 6; i++) pinMode(digitPins[i], OUTPUT);
  pinMode(setButtonPin, INPUT_PULLUP);
  pinMode(plusButtonPin, INPUT_PULLUP);
  pinMode(minusButtonPin, INPUT_PULLUP);
  clearSegments();
  clearDigits();

  blinkTimer = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - blinkTimer > 500) {
    blinkOn = !blinkOn;
    blinkTimer = now;
  }

  // --- Четене на реално време от RTC ---
  if (mode != SET_HOURS && mode != SET_MINUTES && mode != SET_SECONDS) {
    DateTime nowRTC = rtc.now();
    hours = nowRTC.hour();
    minutes = nowRTC.minute();
    seconds = nowRTC.second();
  }

  // --- SET бутон ---
  bool setState = digitalRead(setButtonPin);
  if (setState != lastSetState) {
    lastSetState = setState;
    if (setState == LOW) setPressTime = now;
    else {
      if (!setHeld && now - setPressTime < 1000) {
        if (mode == NORMAL) {
          mode = STOPWATCH;
          stopwatchRunning = false;
        } else if (mode == STOPWATCH) {
          mode = NORMAL;
        } else if (mode == SET_HOURS) {
          mode = SET_MINUTES;
        } else if (mode == SET_MINUTES) {
          mode = SET_SECONDS;
        } else if (mode == SET_SECONDS) {
          mode = NORMAL;

          // Задаване на времето в RTC:
          DateTime current = rtc.now();
          DateTime newTime(current.year(), current.month(), current.day(), hours, minutes, seconds);
          rtc.adjust(newTime);
        }
      }
      setHeld = false;
    }
  }

  if (setState == LOW && now - setPressTime > 1000 && mode == NORMAL) {
    mode = SET_HOURS;
    setHeld = true;
  }

  // --- PLUS бутон ---
  bool plusState = digitalRead(plusButtonPin);
  if (plusState != lastPlusState) {
    lastPlusState = plusState;
    if (plusState == LOW) {
      plusPressTime = now;
      plusHeld = false;
    } else {
      if (!plusHeld && now - plusPressTime < 500) {
        if (mode == SET_HOURS) hours = (hours + 1) % 24;
        else if (mode == SET_MINUTES) minutes = (minutes + 1) % 60;
        else if (mode == SET_SECONDS) seconds = (seconds + 1) % 60;
        else if (mode == STOPWATCH) {
          if (!stopwatchRunning) {
            stopwatchRunning = true;
            stopwatchStartTime = now - stopwatchElapsed;
          } else {
            stopwatchRunning = false;
            stopwatchElapsed = now - stopwatchStartTime;
          }
        }
      }
    }
  }

  if (plusState == LOW && now - plusPressTime > 700) {
    if ((now - plusPressTime) % 500 < DISPLAY_TIME_MS + 1) {
      plusHeld = true;
      if (mode == SET_HOURS) hours = (hours + 1) % 24;
      else if (mode == SET_MINUTES) minutes = (minutes + 1) % 60;
      else if (mode == SET_SECONDS) seconds = (seconds + 1) % 60;
    }
  }

  // --- MINUS бутон ---
  bool minusState = digitalRead(minusButtonPin);
  if (minusState != lastMinusState) {
    lastMinusState = minusState;
    if (minusState == LOW) {
      minusPressTime = now;
      minusHeld = false;
    } else {
      if (!minusHeld && now - minusPressTime < 500) {
        if (mode == SET_HOURS) hours = (hours + 23) % 24;
        else if (mode == SET_MINUTES) minutes = (minutes + 59) % 60;
        else if (mode == SET_SECONDS) seconds = (seconds + 59) % 60;
        else if (mode == STOPWATCH) {
          stopwatchRunning = false;
          stopwatchElapsed = 0;
        }
      }
    }
  }

  if (minusState == LOW && now - minusPressTime > 700) {
    if ((now - minusPressTime) % 500 < DISPLAY_TIME_MS + 1) {
      minusHeld = true;
      if (mode == SET_HOURS) hours = (hours + 23) % 24;
      else if (mode == SET_MINUTES) minutes = (minutes + 59) % 60;
      else if (mode == SET_SECONDS) seconds = (seconds + 59) % 60;
    }
  }

  // --- Изчисляване на стойности за дисплея ---
  unsigned long elapsed = stopwatchElapsed;
  if (mode == STOPWATCH && stopwatchRunning) {
    elapsed = now - stopwatchStartTime;
  }

  clearSegments();
  clearDigits();

  byte digitValue = 0;

  if (mode == STOPWATCH) {
    byte m = (elapsed / 60000UL) % 100;
    byte s = (elapsed / 1000UL) % 60;
    byte ms = (elapsed / 10UL) % 100;

    switch (currentDigit) {
      case 0: digitValue = m / 10; break;
      case 1: digitValue = m % 10; break;
      case 2: digitValue = s / 10; break;
      case 3: digitValue = s % 10; break;
      case 4: digitValue = ms / 10; break;
      case 5: digitValue = ms % 10; break;
    }
  } else {
    switch (currentDigit) {
      case 0: digitValue = hours / 10; break;
      case 1: digitValue = hours % 10; break;
      case 2: digitValue = minutes / 10; break;
      case 3: digitValue = minutes % 10; break;
      case 4: digitValue = seconds / 10; break;
      case 5: digitValue = seconds % 10; break;
    }
  }

  bool shouldDisplay = true;

  if (mode == SET_HOURS && (currentDigit == 0 || currentDigit == 1)) {
    shouldDisplay = blinkOn || plusHeld || minusHeld;
  } else if (mode == SET_MINUTES && (currentDigit == 2 || currentDigit == 3)) {
    shouldDisplay = blinkOn || plusHeld || minusHeld;
  } else if (mode == SET_SECONDS && (currentDigit == 4 || currentDigit == 5)) {
    shouldDisplay = blinkOn || plusHeld || minusHeld;
  } else if (mode != NORMAL && mode != STOPWATCH) {
    shouldDisplay = false;
  }

  if (shouldDisplay) {
    if (!(currentDigit == 0 && digitValue == 0 && mode == NORMAL)) {
      for (byte i = 0; i < 7; i++) {
        digitalWrite(segmentPins[i], digitToSegment[digitValue][i]);
      }
      digitalWrite(digitPins[currentDigit], HIGH);
    }
  }

  currentDigit = (currentDigit + 1) % 6;
  delay(DISPLAY_TIME_MS);
}
