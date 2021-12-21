/*
 * IRremote: IRrecvDemo - demonstrates receiving IR codes with IRrecv
 * An IR detector/demodulator must be connected to the input RECV_PIN.
 * Version 0.1 July, 2009
 * Copyright 2009 Ken Shirriff
 * http://arcfn.com
 */

#include <math.h>
#include <IRremote.h>
#include <stdio.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <PT2258.h>

#define ROT_DT_PIN 2
#define ROT_CLK_PIN 3
#define BUTTON_PIN 4
#define IR_REC_PIN 5

const int MAX_VOL = 100;
const int MIN_VOL = 0;
const int VOL_CHANGE_INCREMENT = 1;
const int MAX_OUTPUT_VOL_VAL = 79;

const int DISPLAY_TIMEOUT = 5000;

PT2258 pt2258;
Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();
IRrecv irrecv(IR_REC_PIN);
decode_results results;

unsigned long last_display_write = 0;

volatile bool muted = false;
int last_change = 0;
bool long_press = false;
bool subseq_long_press = false;
volatile int volume = 0;
volatile bool update_needed = false;
boolean rotClkLastState = LOW;


void setup() {
  Serial.begin(9600);
  Serial.println("Starting setup");
  Wire.setClock(100000);
  Serial.println("Clock set to 100000");
  pinMode(ROT_DT_PIN, INPUT);
  pinMode(ROT_CLK_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(ROT_DT_PIN, HIGH);
  digitalWrite(ROT_CLK_PIN, HIGH);
  digitalWrite(BUTTON_PIN, HIGH);
  Serial.println("Rotary encoder pin setup complete");
  irrecv.enableIRIn();
  SPI.begin();
  Serial.println("SPI enabled");
  alpha4.begin(0x70);  // pass in the address
  alpha4.clear();
  alpha4.writeDisplay();
  Serial.println("Display setup completed");
  if (!pt2258.init()) {
    Serial.println("PT2258 Successfully Initiated");
  } else {
    Serial.println("Failed to Initiate PT2258");
  }

  printTickerMessage("AH SHIT, HERE WE GO AGAIN...", 50);
  printShortMessage("   1", 200);
  printShortMessage("  2 ", 200);
  printShortMessage(" 3  ", 200);
  printShortMessage("4   ", 200);
  printTickerMessage("READY!", 200);
  updateVolume();
  Serial.println("Finished setup");
}

void loop() {
  boolean clkState = digitalRead(ROT_CLK_PIN);
  if (rotClkLastState == HIGH && clkState == LOW) {
    if (digitalRead(ROT_DT_PIN) == LOW) {
      volume = volume - 2;
    } else {
      volume = volume + 2;
    }
    updateVolume();
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    muted = !muted;
    updateVolume();
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(1);
    }
  } else if (irrecv.decode(&results)) {
    Serial.println(results.value);
    int inc = 0;
    int wait_time = 0;
    if (results.value == 3772837903) {
      muted = !muted;
      last_change = 0;
    } else if (!muted) {
      if (results.value == 3772833823) {
        inc = VOL_CHANGE_INCREMENT;
        last_change = inc;
        long_press = false;
        subseq_long_press = false;
      } else if (results.value == 3772829743) {
        inc = -VOL_CHANGE_INCREMENT;
        last_change = inc;
        long_press = false;
        subseq_long_press = false;
      } else if (results.value == 3924233868) {
        if (long_press) {
          inc = last_change * 10;
          if (!subseq_long_press) {
            inc = inc - last_change;
            subseq_long_press = true;
          }
        } else {
          long_press = true;
        }
        wait_time = 150;
      } else {
        last_change = 0;
        long_press = false;
        subseq_long_press = false;
      }
    }
    volume = volume + inc;
    updateVolume();
    delay(wait_time);
    irrecv.resume(); // Receive the next value
  } else {
    handleDisplayTimeout();
  }
  rotClkLastState = clkState;
}

void updateVolume() {
  if (volume > MAX_VOL) {
    volume = MAX_VOL;
  } else if (volume < MIN_VOL) {
    volume = MIN_VOL;
  }
  int vol_to_write;
  if (muted) {
    vol_to_write = convertRegularVolumeToOutputVolume(0);
  } else {
    vol_to_write = convertRegularVolumeToOutputVolume(volume);
  }
  pt2258.setChannelVolume(vol_to_write,0);
  pt2258.setChannelVolume(vol_to_write,1);
  if (muted) {
    char message[] = "Mute";
    printShortMessage(message, 0);
  } else {
    printInteger(volume);
  }
}

int convertRegularVolumeToOutputVolume(int reg_vol) {

  int vol_result = MAX_OUTPUT_VOL_VAL;
  if (reg_vol > 0) {
    double vol_percent = MAX_VOL / (double)reg_vol;
    double float_result = 10 * log(vol_percent) / (double)log(2);
    vol_result = round(float_result);
  }
  
  Serial.print("Display Volume:");
  Serial.print(reg_vol);
  Serial.print("\n");

  Serial.print("Output Volume:");
  Serial.print(vol_result);
  Serial.print("\n");
  return vol_result;
}

void printTickerMessage(char message[], int time_interval) {
  char displaybuffer[] = "    ";
  int message_size = strlen(message);
  for (int i = 0; i < message_size + 3; i++) {
    displaybuffer[0] = displaybuffer[1];
    displaybuffer[1] = displaybuffer[2];
    displaybuffer[2] = displaybuffer[3];
    if (i < message_size - 1) {
      displaybuffer[3] = message[i];
    } else {
      displaybuffer[3] = ' ';
    }
    alpha4.writeDigitAscii(0, displaybuffer[0], displaybuffer[0] == '.');
    alpha4.writeDigitAscii(1, displaybuffer[1], displaybuffer[1] == '.');
    alpha4.writeDigitAscii(2, displaybuffer[2], displaybuffer[2] == '.');
    alpha4.writeDigitAscii(3, displaybuffer[3], displaybuffer[3] == '.');
    alpha4.writeDisplay();
    delay(time_interval);
  }
  alpha4.clear();
  alpha4.writeDisplay();
  last_display_write = millis();
  
}

void printShortMessage(char message[], int delay_ms) {
  alpha4.clear();
  int message_len = strlen(message);
  for (int i = 0; i < 4 && i < message_len; i++) {
    alpha4.writeDigitAscii(i, message[i]);
  }
  alpha4.writeDisplay();
  last_display_write = millis();
  delay(delay_ms);
}

void printInteger(int number) {
  alpha4.clear();
  int volume_to_print = volume * 100 / MAX_VOL;
  for (int i = 3; (i == 3 || volume_to_print > 0) && i >= 0; i--) {
    alpha4.writeDigitAscii(i, (volume_to_print % 10) + 48);
    volume_to_print = volume_to_print / 10;
  }
  alpha4.writeDisplay();
  last_display_write = millis();
}

void handleDisplayTimeout() {
  if (!muted && last_display_write > 0 && millis() - last_display_write > DISPLAY_TIMEOUT) {
    alpha4.clear();
    alpha4.writeDisplay();
    last_display_write = 0;
  }
}
