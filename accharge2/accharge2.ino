#include <Arduino.h>
#include <SoftwareSerial.h>
#include "PWM.h"
#include <SPI.h>
#include <Wire.h>
#include "EmonLib.h"

// #define DEBUG
EnergyMonitor emon1;
EnergyMonitor emon2;
EnergyMonitor emon3;

#define CP_OUT 10
#define CP_IN A0

#define rxPin 6
#define txPin 5
#define temperaturepin A7

SoftwareSerial mySerial(rxPin, txPin);
String receivedData = "";

const int SAMPLES = 1000;
int sample_count = 0;
int peak_voltage = 0;

double current1 = 0;
double current2 = 0;
double current3 = 0;
int voltage1 = 0;
int voltage2 = 0;
int voltage3 = 0;
int temperature = 0;

int critical_num = 42;

int counter = 0;
int counter2 = 0;

float cp_pwm = 255;
int frequency = 1000;

void findPeakVoltage();
int peak_voltage = 0;
int current_voltage = 0;
byte Read_Pilot_Voltage();
char state = 'z';

int charging = 0;

double Irms;
#define POS_12V_MAX 1024 // 12V
#define POS_12V_MIN 925  // 11V
#define POS_9V_MAX 924   // 10V
#define POS_9V_MIN 876   // 8V
#define POS_6V_MAX 875   // 7V
#define POS_6V_MIN 735   // 5V
#define POS_3V_MAX 708   // 4V
#define POS_3V_MIN 656   // 2V

#define STATE_A 1 // No EV connected
#define STATE_B 2 // EV Connected but not charging
#define STATE_C 3 // Charging

#define _12_VOLTS 12
#define _9_VOLTS 9
#define _6_VOLTS 6
#define _3_VOLTS 3

#define amper_state10 42
#define amper_state16 68
#define amper_state24 102
#define amper_state32 135

#define phase1_current A1
#define phase2_current A6
#define phase3_current A2

#define relay8 8
#define relay9 9

void setup()
{
  Serial.begin(9600);
  mySerial.begin(57600);

  InitTimersSafe();
  bool success = SetPinFrequencySafe(CP_OUT, frequency);
  if (success)
  {
    pinMode(CP_OUT, OUTPUT);
  }
  
  pinMode(relay8, OUTPUT);
  pinMode(relay9, OUTPUT);

  digitalWrite(relay9, LOW);
  digitalWrite(relay8, LOW);

  emon1.current(phase1_current, 25);
  emon2.current(phase2_current, 25);
  emon3.current(phase3_current, 25);
}

void loop()
{
  static byte Current_State = STATE_A;
  static byte PVC;

  findPeakVoltage();
  if(sample_count == 0) {
    PVC = Read_Pilot_Voltage();
  }
  
  current1 = emon1.calcIrms(1480);
  current2 = emon2.calcIrms(1480);
  current3 = emon3.calcIrms(1480);

  /*   voltage1 = emon1.Vrms;
    voltage2 = emon2.Vrms;
    voltage3 = emon3.Vrms; */

  if (mySerial.available())
  {
    char c = mySerial.read();
    receivedData += c;
    Serial.print(receivedData);
    if (c == '\n')
    {
#ifdef DEBUG
      mySerial.println(receivedData);
#endif
      parsedata(receivedData);
      receivedData = "";
    }
  }
  send_data();
  read_temperature();

#ifdef DEBUG
  // Serial.println(analogRead(A2));
  Serial.print("cu1=");
  Serial.println(current1);
  Serial.print("cu2=");
  Serial.println(current2);
  Serial.print("cu3=");
  Serial.println(current3);
  Serial.print("vo1=");
  Serial.println(voltage1);
  Serial.print("vo2=");
  Serial.println(voltage2);
  Serial.print("vo3=");
  Serial.println(voltage3);
  Serial.print("Temperature= ");
  Serial.print(temperature);
#endif

  counter++;
  if (counter > 10)
  {
    counter = 0;
    Serial.print("Peak Voltage: ");
    Serial.println(peak_voltage);
    Serial.println(Current_State);
  }

  if (PVC == _9_VOLTS or PVC == _6_VOLTS or PVC == _3_VOLTS)
  {
    charging = 0;
    pwmWrite(CP_OUT, critical_num); // 10 A -> Duty Cycle = 10/0.6 = 16.67% -> 255 * 0.1667 = 42.5
    digitalWrite(relay9, LOW);      // Relays open  //16 A -> Duty Cycle = 16/0.6 = 26.67% -> 255 * 0.2667 = 68
    digitalWrite(relay8, LOW);      // 24 A -> Duty Cycle = 24/0.6 = 40.00% -> 255 * 0.4000 = 102
    digitalWrite(2, HIGH);          // Bottom LED on     //32 A -> Duty Cycle = 32/0.6 = 53.33% -> 255 * 0.5333 = 135
    digitalWrite(3, LOW);           // Middle d LED off
    digitalWrite(4, LOW);           // Middle u LED off
    Current_State = STATE_B;
  }

  if (PVC == _12_VOLTS)
  {
    charging = 0;
    pwmWrite(CP_OUT, 255);     // CP set to fixed +12V
    digitalWrite(relay9, LOW); // Relays open
    digitalWrite(relay8, LOW);
    digitalWrite(2, LOW); // Bottom LED off
    digitalWrite(3, LOW); // Middle d LED off
    digitalWrite(4, LOW); // Middle u LED off
    Serial.println("State A");
    Current_State = STATE_A;
  }
  else if (PVC == _6_VOLTS or PVC == _3_VOLTS)
  {
    charging = 1;
    pwmWrite(CP_OUT, critical_num);
    digitalWrite(relay8, HIGH); // Relays closed
    digitalWrite(relay9, HIGH);
    digitalWrite(2, HIGH); // Bottom LED on
    Serial.println("State C");
    Current_State = STATE_C;

    /*     if(analogRead(A6) != 0) {
              Serial.println("GFCI_TEST_FAIL");}
        else  {
                 charging = 1;
        pwmWrite(CP_OUT,42);
        digitalWrite(9, HIGH); // Relays closed
        digitalWrite(2, HIGH); // Bottom LED on
              Serial.println("State C");
              Current_State = STATE_C;
            } */
  }

  if (PVC == _12_VOLTS or PVC == _9_VOLTS)
  {
    charging = 0;
    digitalWrite(relay9, LOW); // Relays open
    digitalWrite(relay8, LOW);
    Serial.println("State B");
    Current_State = STATE_B;
  }
}

void findPeakVoltage()
{

  if (sample_count < SAMPLES)
  {
    current_voltage = analogRead(CP_IN);
    if (current_voltage > peak_voltage)
    {
      peak_voltage = current_voltage;
    }
    sample_count++;
  }
  else
  {
    sample_count = 0;
  }
}

byte Read_Pilot_Voltage()
{
  static int Previous_Value = _12_VOLTS;
  if (peak_voltage > POS_12V_MIN and peak_voltage < POS_12V_MAX)
  {
    Previous_Value = _12_VOLTS;
    return (_12_VOLTS);
  }
  else if (peak_voltage > POS_9V_MIN and peak_voltage < POS_9V_MAX)
  {
    Previous_Value = _9_VOLTS;
    return (_9_VOLTS);
  }
  else if (peak_voltage > POS_6V_MIN and peak_voltage < POS_6V_MAX)
  {
    Previous_Value = _6_VOLTS;
    return (_6_VOLTS);
  }
  else if (peak_voltage > POS_3V_MIN and peak_voltage < POS_3V_MAX)
  {
    Previous_Value = _3_VOLTS;
    return (_3_VOLTS);
  }
  else
  {
    return (Previous_Value);
  }
}

void parsedata(String data)
{
  if (data.startsWith("s"))
  {
    int coming_ampere = data.substring(1).toInt();
    switch (coming_ampere)
    {
    case 0:
      critical_num = amper_state10;
      break;
    case 1:
      critical_num = amper_state16;
      break;
    case 2:
      critical_num = amper_state24;
      break;
    case 3:
      critical_num = amper_state32;
      break;
    }
#ifdef DEBUG
    Serial.print("Critical number: ");
    Serial.println(critical_num);
#endif
  }
}

void send_data()
{
  mySerial.print("c1");
  mySerial.println(current1, 2);
  mySerial.print("c2");
  mySerial.println(current2, 2);
  mySerial.print("c3");
  mySerial.println(current3, 2);
  /*   mySerial.print("v1");
    mySerial.println(voltage1);
    mySerial.print("v2");
    mySerial.println(voltage2);
    mySerial.print("v3");
    mySerial.println(voltage3);
    mySerial.print("pv");
    mySerial.println(peak_voltage); */
  mySerial.print("te");
  mySerial.println(temperature);
  mySerial.print("cr");
  mySerial.println(critical_num);
}

void read_temperature()
{
  int sensorValue = analogRead(temperaturepin);
  float voltagetemp = sensorValue * (5.0 / 1023.0);
  temperature = voltagetemp * 100;
}
