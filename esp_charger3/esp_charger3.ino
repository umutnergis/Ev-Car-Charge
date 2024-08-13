#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <string.h>
#include "esp_system.h"

//#define DEBUG
#define check10A 0x20 
#define check16A 0x21 
#define check24A 0x22 
#define check32A 0x23

#define RXp2 16
#define TXp2 17

TaskHandle_t Task1;
TaskHandle_t Task2;

const char *ssid = "Şarj_Global";
const char *password = "12345678";
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer server(80);

String receivedData = "";
int kwh = 0;
float temp;
int voltage1 = 0, voltage2 = 0, voltage3 = 0;
double current1 = 0, current2 = 0, current3 = 0;
int temperature = 0;
int coming_pvc = 0;
int coming_critical = 0;
unsigned long previousMillis = 0;

String charge_sit = "NO";
String amper_sit = "0 A";

void thread1(void* pvParameters);
void thread2(void* pvParameters);

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXp2, TXp2);
  delay(500);

  xTaskCreatePinnedToCore(thread1, "Task1", 10000, NULL, 1, &Task1, 0);
  xTaskCreatePinnedToCore(thread2, "Task2", 5000, NULL, 1, &Task2, 1);

  acces_point();
}

void loop() {

}

void thread1(void* pvParameters) {
  for(;;) {
    
    if(Serial2.available()) {
      char c = Serial2.read();
      receivedData += c;
      if (c == '\n') {
        parsedata(receivedData);
        receivedData = "";
      }
    }

    kwh = ((230*current1*0.9)+(230*current2*0.9)+(230*current3*0.9)); 
    
    if(Serial.available()) {
      byte incomingByte = Serial.read();
      check_button(incomingByte);
    }
    sendFunction();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void thread2(void* pvParameters) {
  for(;;) {
    dnsServer.processNextRequest();
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
    temp = temperatureRead();  
  }

}

void acces_point() {

  int bestChannel = 1;
  long bestRSSI = -100;
  for (int channel = 1; channel <= 11; channel++) {
    int n = WiFi.scanNetworks(false, false, false, 300, channel);
    for (int i = 0; i < n; i++) {
      if (WiFi.RSSI(i) > bestRSSI) {
        bestRSSI = WiFi.RSSI(i);
        bestChannel = channel;
      }
    }
  }

  // Set WiFi mode and increase power
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  // Start AP on the best channel
  WiFi.softAP(ssid, password, bestChannel);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  
  Serial.println("Access Point Başlatıldı");
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP adresi: ");
  Serial.println(IP);

  dnsServer.start(DNS_PORT, "*", IP);

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/number", handleNumber);
  server.on("/button1", handleButton1);
  server.on("/button2", handleButton2);
  server.on("/button3", handleButton3);
  server.on("/button4", handleButton4);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);

  server.begin();
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Elektrikli Araç Şarj Göstergesi</title>
    <style>
        body{font-family:Arial,sans-serif;text-align:center;margin-top:75px}.number{font-size:48px;color:#333;margin-bottom:20px}.button-container{display:flex;justify-content:center;gap:20px;margin-top:30px}.button,.control-button,.submit-button{padding:15px 30px;font-size:18px;cursor:pointer;border:none;border-radius:5px;color:#fff}.button{background-color:#0373fc}.control-button{background-color:#2196F3}.start-button{background-color:#4CAF50}.stop-button{background-color:#f44336}.status-container{margin-top:30px}input[type=text]{font-size:18px;padding:10px;width:80%;max-width:400px;margin-bottom:20px;border:2px solid #ccc;border-radius:5px;text-align:center}input[type=text]:disabled{background-color:#f9f9f9;color:#333}
    </style>
</head>
<body>
    <h1>KWH Göstergesi</h1>
    <div class="number" id="number">0</div>
    <div class="button-container">
        <button class="button" onclick="sendCommand('button1')">10 A</button>
        <button class="button" onclick="sendCommand('button2')">16 A</button>
        <button class="button" onclick="sendCommand('button3')">24 A</button>
        <button class="button" onclick="sendCommand('button4')">32 A</button>
    </div>
    <div class="button-container">
        <button class="control-button start-button" onclick="sendCommand('start')">Başlat</button>
        <button class="control-button stop-button" onclick="sendCommand('stop')">Durdur</button>
    </div>
    <div class="status-container">
        <input type="text" id="statusInput" value="Durum Bekleniyor..." disabled>
    </div>
    <script>
        function fetchNumber(){fetch('/number').then(e=>e.text()).then(e=>{document.getElementById('number').innerText=e}).catch(e=>console.error('Hata:',e))}setInterval(fetchNumber,500);function fetchStatus(){fetch('/status').then(e=>e.text()).then(e=>{document.getElementById('statusInput').value=e}).catch(e=>console.error('Durum alma hatası:',e))}setInterval(fetchStatus,200);function sendCommand(e){fetch('/'+e).then(e=>e.text()).then(t=>{console.log('Komut gönderildi:',e)}).catch(e=>console.error('Komut gönderme hatası:',e))}
    </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleNumber() {
  server.send(200, "text/plain", String(kwh));
}

void handleStatus() {
  String data = ("CCri " + coming_critical + " Aktif Amper " + amper_sit + "Sıcaklık " +temp);
  server.send(200, "application/json", data);
}

void handleButton1() {
  amper_sit = "10 A";
  Serial2.println("s0");
  server.send(200, "text/plain", "OK");
}

void handleButton2() {
  amper_sit = "16 A";
  Serial2.println("s1");
  server.send(200, "text/plain", "OK");
}

void handleButton3() {
  amper_sit = "24 A";
  Serial2.println("s2");
  server.send(200, "text/plain", "OK");
}

void handleButton4() {
  amper_sit = "32 A";
  Serial2.println("s3");
  server.send(200, "text/plain", "OK");
}

void handleStart() {
  charge_sit = "ON";
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  charge_sit = "OFF";
  server.send(200, "text/plain", "OK");
}

void sendNumberToVP(byte vp1, byte vp2, int number) {
  byte num_high_byte = (number >> 8) & 0xFF;
  byte num_low_byte = number & 0xFF;
  
  byte packet[] = {0x5A, 0xA5, 0x06, 0x82, vp1, vp2, num_high_byte, num_low_byte};

  for (byte i = 0; i < sizeof(packet); i++) {
    Serial.write(packet[i]);
  }
}

void sendFunction() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 300) {
    previousMillis = currentMillis;
    sendNumberToVP(0x50, 0x00, kwh);
    #ifdef DEBUG
    Serial.println("Data sent to VP");
    #endif
  }
}

void check_button(byte coming) {
  switch(coming) {
    case check10A:
      Serial2.println("s0");
      amper_sit = "10 A";
      #ifdef DEBUG
      Serial.println("10A buton algılandı");
      #endif
      break;
    case check16A:
      Serial2.println("s1");
      amper_sit = "16 A";
      #ifdef DEBUG
      Serial.println("16A buton algılandı");
      #endif
      break;
    case check24A:
      Serial2.println("s2");
      amper_sit = "24 A";
      #ifdef DEBUG
      Serial.println("24A buton algılandı");
      #endif
      break;
    case check32A:
      Serial2.println("s3");
      amper_sit = "32 A";
      #ifdef DEBUG
      Serial.println("32A buton algılandı");
      #endif
      break;
  }
}

void parsedata(String data) {
  if (data.startsWith("v1")) {
    voltage1 = data.substring(2).toInt();
    #ifdef DEBUG
    Serial.print("Voltage 1: ");
    Serial.println(voltage1);
    #endif
  } 
  else if (data.startsWith("v2")) {
    voltage2 = data.substring(2).toInt();
    #ifdef DEBUG
    Serial.print("Voltage 2: ");
    Serial.println(voltage2);
    #endif
  } 
  else if (data.startsWith("v3")) {
    voltage3 = data.substring(2).toInt();
    #ifdef DEBUG
    Serial.print("Voltage 3: ");
    Serial.println(voltage3);
    #endif
  } 
  else if (data.startsWith("c1")) {
    current1 = data.substring(2).toFloat();
    #ifdef DEBUG
    Serial.print("Current 1: ");
    Serial.println(current1);
    #endif
  } 
  else if (data.startsWith("c2")) {
    current2 = data.substring(2).toFloat();
    #ifdef DEBUG
    Serial.print("Current 2: ");
    Serial.println(current2);
    #endif
  } 
  else if (data.startsWith("c3")) {
    current3 = data.substring(2).toFloat();
    #ifdef DEBUG
    Serial.print("Current 3: ");
    Serial.println(current3);
    #endif
  }
  else if (data.startsWith("te")) {
    temperature = data.substring(2).toInt();
    #ifdef DEBUG
    Serial.print("Temperature: ");
    Serial.println(temperature);
    #endif
  }
  /* else if (data.startsWith("pv")) {
    coming_pvc = data.substring(2).toInt();
    #ifdef DEBUG
    Serial.print("Peak Voltage: ");
    Serial.println(coming_pvc);
    #endif
  } */
  else if (data.startsWith("cr")) {
    coming_critical = data.substring(2).toInt();
    #ifdef DEBUG
    Serial.print("Critical number: ");
    Serial.println(coming_critical);
    #endif
  }
}