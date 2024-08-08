#include <Arduino.h>
#include <SoftwareSerial.h>
#include "PWM.h"
#include <SPI.h>
#include <Wire.h>
#include "EmonLib.h"     

#define DEBUG
EnergyMonitor emon1;
EnergyMonitor emon2; 
EnergyMonitor emon3;

#define CP_OUT 10
#define CP_IN A0

#define rxPin 6
#define txPin 5
#define tempertaturepin A7

SoftwareSerial mySerial(rxPin, txPin); 
String receivedData = "";

int current1 = 0;
int current2 = 0;
int current3 = 0;
int voltage1 = 0;
int voltage2 = 0;
int voltage3 = 0;
int temperature =0;

int critical_num = 42;

int counter = 0;
int counter2 = 0;

float cp_pwm = 255;
int frequency = 1000;

int findPeakVoltage();
int peak_voltage = 0;
int current_voltage = 0;
byte Read_Pilot_Voltage();
char state = 'z';

int charging = 0;

double Irms;      
#define         POS_12V_MAX                 1000                                 // 12V
#define         POS_12V_MIN                 800                                 // 11V
#define         POS_9V_MAX                  799                                 // 10V        
#define         POS_9V_MIN                  650                                 // 8V
#define         POS_6V_MAX                  649                                 // 7V
#define         POS_6V_MIN                  341                                 // 5V
#define         POS_3V_MAX                  273                                 // 4V  
#define         POS_3V_MIN                  136                                // 2V

#define         STATE_A                     1                                   // No EV connected
#define         STATE_B                     2                                   // EV Connected but not charging
#define         STATE_C                     3                                   // Charging
//#define         CHARGE_PWM_DUTY             350

#define         _12_VOLTS                   12
#define         _9_VOLTS                    9
#define         _6_VOLTS                    6
#define         _3_VOLTS                    3

#define relay8 8
#define relay9 9


  
void setup() { 
  Serial.begin(9600);
  mySerial.begin(9600);

  emon1.current(A1, 111.1);
  emon2.current(A2, 111.1);
  emon3.current(A6, 111.1);
 
  emon1.voltage(A3, 234.6, 1.7); // Voltage: (input pin, calibration, phase_shift)
  emon2.voltage(A4, 234.6, 1.7); // Voltage: (input pin, calibration, phase_shift)
  emon3.voltage(A5, 234.6, 1.7); // Voltage: (input pin, calibration, phase_shift)


  InitTimersSafe();
  bool success = SetPinFrequencySafe(CP_OUT, frequency);
  if(success) {
    pinMode(CP_OUT, OUTPUT);  
  }

  pinMode(4, OUTPUT);  
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);


  pinMode(relay8, OUTPUT);
  pinMode(relay9, OUTPUT);

  digitalWrite(relay9, LOW); // Relays open
  digitalWrite(relay8, LOW); // Relays open
  digitalWrite(2, LOW); // Bottom LED off
}

void loop() {
static byte Current_State = STATE_A;
static byte PVC;
findPeakVoltage();
PVC = Read_Pilot_Voltage(); 

  current1 = emon1.Irms;
  current2 = emon2.Irms;
  current3 = emon3.Irms;

  voltage1 = emon1.Vrms;
  voltage2 = emon2.Vrms;
  voltage3 = emon3.Vrms;

    if(mySerial.available())
  {
    Serial.println("Serial başladı");
    char c = mySerial.read();
    receivedData += c;

    if (c == '\n') {
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
  Serial.println(analogRead(A2));
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
  if (counter > 10) {
    counter = 0;
    Serial.println("contador = 10");
    Serial.print("Peak Voltage: ");
    Serial.println(peak_voltage);
    Serial.println(Current_State);
  }
  if (charging == 1) {
    counter2++;
    if (counter2 > 10) {
      counter2 = 0;
      //if digitalwrite 3 is high, turn it off
      if (digitalRead(3) == HIGH) {
        digitalWrite(3, LOW);
        digitalWrite(4, HIGH);
      }
      else {
        digitalWrite(3, HIGH);
        digitalWrite(4, LOW);
      }
    }
  }
  

      if(PVC == _9_VOLTS or PVC == _6_VOLTS or PVC == _3_VOLTS)  {
          charging = 0;
    pwmWrite(CP_OUT,critical_num); // 10 A -> Duty Cycle = 10/0.6 = 16.67% -> 255 * 0.1667 = 42.5
    digitalWrite(relay9, LOW); // Relays open  //16 A -> Duty Cycle = 16/0.6 = 26.67% -> 255 * 0.2667 = 68
    digitalWrite(relay8, LOW);                  //24 A -> Duty Cycle = 24/0.6 = 40.00% -> 255 * 0.4000 = 102
    digitalWrite(2, HIGH); // Bottom LED on     //32 A -> Duty Cycle = 32/0.6 = 53.33% -> 255 * 0.5333 = 135
    digitalWrite(3, LOW); // Middle d LED off
    digitalWrite(4, LOW); // Middle u LED off       
        Current_State = STATE_B;
      }    
     
      
      
      if(PVC == _12_VOLTS)  {
         charging = 0;
    pwmWrite(CP_OUT,255); // CP set to fixed +12V
    digitalWrite(relay9, LOW); // Relays open
    digitalWrite(relay8, LOW);
    digitalWrite(2, LOW); // Bottom LED off
    digitalWrite(3, LOW); // Middle d LED off
    digitalWrite(4, LOW); // Middle u LED off
        Serial.println("State A");
        Current_State = STATE_A;
      }     
      else if(PVC == _6_VOLTS or PVC == _3_VOLTS)  {
        charging = 1;
        pwmWrite(CP_OUT,critical_num);
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
     
      if(PVC == _12_VOLTS or PVC == _9_VOLTS)  {
        charging = 0;
        digitalWrite(relay9, LOW); // Relays open
        digitalWrite(relay8, LOW);
        Serial.println("State B");        
        Current_State = STATE_B;
      }


}
// function that finds the peak voltage read by pin A0
  int findPeakVoltage() {
    int i = 0;
    current_voltage = 0;
    peak_voltage = 0;
    while (i < 1000) {
      current_voltage = analogRead(CP_IN);
      if (current_voltage > peak_voltage) {
        peak_voltage = current_voltage;
      }
      i++;
    }
    return peak_voltage;
  }

  byte Read_Pilot_Voltage()
{
 
  static int Previous_Value = _12_VOLTS;

 
    if(peak_voltage > POS_12V_MIN and peak_voltage < POS_12V_MAX) {
      Previous_Value = _12_VOLTS; 
      return(_12_VOLTS); 
    }
    else if(peak_voltage > POS_9V_MIN and peak_voltage < POS_9V_MAX) {
      Previous_Value = _9_VOLTS; 
      return(_9_VOLTS);
    }
    else if(peak_voltage > POS_6V_MIN and peak_voltage < POS_6V_MAX) {
      Previous_Value = _6_VOLTS; 
      return(_6_VOLTS);
    }
    else if(peak_voltage > POS_3V_MIN and peak_voltage < POS_3V_MAX) {
      Previous_Value = _3_VOLTS; 
      return(_3_VOLTS);
    }
    else {
        return(Previous_Value);
    }
}

void parsedata(String data) {
  if(data.startsWith("st")) {
    critical_num = data.substring(2).toInt();
    Serial.print("Critical number: ");
    Serial.println(critical_num);
  }
}


void send_data() {
  mySerial.print("cu1=");
  mySerial.println(current1);
  mySerial.print("cu2=");
  mySerial.println(current2);
  mySerial.print("cu3=");
  mySerial.println(current3);
  mySerial.print("vo1=");
  mySerial.println(voltage1);
  mySerial.print("vo2=");
  mySerial.println(voltage2);
  mySerial.print("vo3=");
  mySerial.println(voltage3);
  mySerial.print("tem=");
  mySerial.println(temperature);
}

void read_temperature()
{
int sensorValue = analogRead(tempertaturepin);
float voltagetemp = sensorValue * (5.0 / 1023.0);
temperature = voltagetemp * 100;
}
