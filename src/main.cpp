#include <RTClib.h>
#include <SPI.h>
#include <RotaryEncoder.h>
#include <TaskScheduler.h>

#include "OneButton.h"
#include "I2C_LCD.h"

#define ENCODER_CLK_PIN 4
#define ENCODER_DT_PIN 5
#define ENCODER_SW_PIN 3

RTC_DS3231 rtc;
I2C_LCD lcd(39);
RotaryEncoder encoder(ENCODER_CLK_PIN, ENCODER_DT_PIN, RotaryEncoder::LatchMode::FOUR3);
Scheduler runner;
OneButton encoderButton(ENCODER_SW_PIN, true);

static int lastPos = 0;
static bool editMode = false;

static DateTime alarmTime;

void updateDisplay();
Task updateDisplayTask(1000, TASK_FOREVER, &updateDisplay);

void restartBacklightTimer();
void turnOffBacklight();
Task turnOffBacklightTask(0, TASK_ONCE, &turnOffBacklight);

void handleEncoder();
void onEncoderClick();

void onAlarm();

void setup() {
    Serial.begin(9600);

    // initializing the lcd
    lcd.begin(16, 2);

    if (!lcd.isConnected()) {
      Serial.println("Couldn't find display!");
      Serial.flush();
      while (1) delay(10);
    }
    lcd.display();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Initializing...");

    // initializing the rtc
    if(!rtc.begin()) {
      Serial.println("Couldn't find RTC!");
      Serial.flush();
      while (1) delay(10);
    }

    //we don't need the 32K Pin, so disable it
    rtc.disable32K();

    encoderButton.attachClick(onEncoderClick);

    // set alarm 1, 2 flag to false (so alarm 1, 2 didn't happen so far)
    // if not done, this easily leads to problems, as both register aren't reset on reboot/recompile
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);

    // stop oscillating signals at SQW Pin
    // otherwise setAlarm1 will fail
    rtc.writeSqwPinMode(DS3231_OFF);

    // turn off alarm 2 (in case it isn't off already)
    // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
    rtc.disableAlarm(2);

    // schedule an alarm 5 minutes in the future as a default starting point
    alarmTime = rtc.now() + TimeSpan(5 * 60) - TimeSpan(rtc.now().second());
    if(!rtc.setAlarm1(
            alarmTime,
            DS3231_A1_Hour // this mode triggers the alarm when the minutes match
    )) {
        Serial.println("Error, default alarm wasn't set!");
    }

    runner.init();

    // add tasks to scheduler
    runner.addTask(updateDisplayTask);
    runner.addTask(turnOffBacklightTask);

    delay(5000);
    updateDisplayTask.enable();
    restartBacklightTimer();
}

void loop() {
  // check if alarm is going off
  if (rtc.alarmFired(1)) {
    onAlarm();

    while (true) {
      delay(10);
    }
  } else {
    encoderButton.tick();
    runner.execute();

    if (editMode) {
      handleEncoder();
    }
  }
}

void updateDisplay() {
  // print current time to display
  char time[10] = "hh:mm:ss";
  rtc.now().toString(time);
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("Current Time:");
  lcd.setCursor(4,1);
  lcd.print(time);
}

void displayAlarmTime() {
  char time[10] = "hh:mm:ss";
  alarmTime.toString(time);
  lcd.clear();
  lcd.setCursor(3,0);
  lcd.print("Alarm Time:");
  lcd.setCursor(4,1);
  lcd.print(time);
}

void restartBacklightTimer() {
  lcd.setBacklight(true);
  turnOffBacklightTask.restartDelayed(5000);
}

void turnOffBacklight() {
  lcd.setBacklight(false);
}

void handleEncoder() {
  encoder.tick();
  int newPos = encoder.getPosition();

  if (lastPos != newPos) {
    alarmTime = rtc.getAlarm1() + TimeSpan(newPos * 60);

    restartBacklightTimer();
    displayAlarmTime();
  }

  lastPos = newPos;
}

void onEncoderClick() {
  restartBacklightTimer();

  editMode = !editMode;

  if (editMode) {
    updateDisplayTask.disable();

    displayAlarmTime();
  } else {
    updateDisplayTask.enable();

    // update alarm time on rtc module
    rtc.setAlarm1(
      rtc.getAlarm1() + TimeSpan(lastPos * 60),
      DS3231_A1_Hour
    );

    char time[10] = "hh:mm:ss";
    Serial.print("New Alarm Time: ");
    rtc.getAlarm1().toString(time);
    Serial.println(time);

    // reset encoder position
    lastPos = 0;
  }
}

void onAlarm() {
  Serial.println("Alarm fired!");

  lcd.setBacklight(true);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Alarm Going Off!");
  lcd.setCursor(4,1);
  lcd.print("Wake up!");

  // TODO put mp3 code here
}