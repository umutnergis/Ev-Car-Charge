#include <WiFi.h>
#include <WebServer.h>
#include <string.h>

#define DEBUG
#define check10A 0x20 
#define check16A 0x21 
#define check24A 0x22 
#define check32A 0x23

#define RXp2 16
#define TXp2 17

TaskHandle_t Task1;
TaskHandle_t Task2;

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 30000;

const char* ssid = "xxxxxx";
const char* password = "xxxxxx";
const char* hostname = "EV-Charger";
WebServer server(80);
int check_internet = 0;

unsigned long previousMillis = 0;
unsigned long currentMillis =0;


String receivedData = "";
int kwh = 0;

int voltage1=0;
int voltage2=0;
int voltage3=0;

int current1=0;
int current2=0;
int current3=0;
int temperature=0;

void thread1(void* pvParameters);
void thread2(void* pvParameters);

bool connect_wifi() {
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);
  #ifdef DEBUG
  Serial.print("Wi-Fi'ye bağlanılıyor");
  #endif
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { 
    delay(1000);
    #ifdef DEBUG
    Serial.print(".");
    #endif
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    #ifdef DEBUG
    Serial.println("\nWi-Fi'ye bağlanıldı!");
    Serial.print("IP Adresi: ");
    Serial.println(WiFi.localIP());
    #endif
    return true;
  } else {
    #ifdef DEBUG
    Serial.println("\nWi-Fi bağlantısı başarısız oldu.");
    #endif
    return false;
  }
}


void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXp2, TXp2);
  delay(500);
 //thread1 main //thread2 html
  xTaskCreatePinnedToCore(
    thread1,   // Function to implement the task
    "Task1",   // Name of the task
    10000,     // Stack size in words
    NULL,      // Task input parameter
    1,         // Priority of the task
    &Task1,    // Task handle
    0);        // Core where the task should run

  xTaskCreatePinnedToCore(
    thread2,   // Function to implement the task
    "Task2",   // Name of the task
    5000,     // Stack size in words
    NULL,      // Task input parameter
    1,         // Priority of the task
    &Task2,    // Task handle
    1);        // Core where the task should run
}


void loop() {
  
}


void thread2(void* pvParameters)
{
  #ifdef DEBUG
  Serial.println("Thread2 başladı");
  #endif
for(;;)
{
  if (WiFi.status() != WL_CONNECTED) {
    // Internet connection is lost
    check_internet = 0;
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
      lastReconnectAttempt = currentMillis;
      if (connect_wifi()) {
        // Successfully reconnected
        server.on("/", HTTP_GET, handleRoot);
        server.on("/number", HTTP_GET, handleNumber);
        server.begin();
        check_internet = 1;
        #ifdef DEBUG
        Serial.println("Server Başlatıldı");
        #endif
      }
    }
  }
  
  if (check_internet == 1) {
    server.handleClient();
  }
  vTaskDelay(pdMS_TO_TICKS(300));
 }
}

void thread1(void* pvParameters)
{
  #ifdef DEBUG
  Serial.println("Thread1 başladı");
  #endif 
for(;;)
{
  //Serial1 = ekran ile Serial2 = arduino ile haberleşiyorsun...
  if(Serial2.available())
  {
    char c = Serial2.read();
    receivedData += c;

    if (c == '\n') {
      #ifdef DEBUG
      Serial2.println(receivedData);
      #endif
      parsedata(receivedData);
      receivedData = "";
    }
  }

  kwh = ((voltage1*current1*0.9)+(voltage2*current2*0.9)+(voltage3*current3*0.9)); 
  if(Serial.available())
  {
    
    byte incomingByte = Serial.read();
    #ifdef DEBUG
    Serial.println(incomingByte, HEX);
    #endif
    check_button(incomingByte);
  }
  currentMillis = millis();
  sendFunction();

  vTaskDelay(pdMS_TO_TICKS(50));
 }

}



void handleRoot() {
  server.send(200, "text/html", R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Elektrikli Araç Şarj Göstergesi</title>
        <style>
        
            body {
                font-family: Arial, sans-serif;
                text-align: center;
                margin-top: 50px;
            }
            .number {
                font-size: 48px;
                color: #333;
            }
        </style>
    </head>
    <body>
        <h1> KWH Göstergesi</h1>
        <div class="number" id="number">0</div>
        <script>
            // Bu kısım, ESP32'den sayıyı almak için bir HTTP isteği yapar.
            function fetchNumber() {
                fetch('/number')
                    .then(response => response.text())
                    .then(data => {
                        document.getElementById('number').innerText = data;
                    })
                    .catch(error => console.error('Hata:', error));
            }
            setInterval(fetchNumber, 300);
        </script>
    </body>
    </html>
  )");
}


void handleNumber() {
  server.send(200, "text/plain", String(kwh));
}

void sendNumberToVP(byte vp1,byte vp2, int number) {
  byte num_high_byte = (number >> 8) & 0xFF;
  byte num_low_byte = number & 0xFF;
  
  byte packet[] = {0x5A, 0xA5, 0x06, 0x82, vp1, vp2, num_high_byte, num_low_byte};

  for (byte i = 0; i < sizeof(packet); i++) {
    Serial.write(packet[i]);
  }
}

void sendFunction()
{
  unsigned long currentMillis=millis();
  if (currentMillis - previousMillis >= 300) {
  previousMillis = currentMillis;
  sendNumberToVP(0x50, 0x00, kwh);
  }
}

void check_button(byte coming)
{

  switch(coming)
  {
    case check10A:
    Serial2.println("st10");
    break;

    case check16A:
    Serial2.println("st68");
    break;

    case check24A:
    Serial2.println("st102");
    break;

    case check32A:
    Serial2.println("st135");
    break;
  }

}

void parsedata(String data) {

/*   if (data.startsWith("kwh=")) {
    kwh = data.substring(4).toInt(); 
    #ifdef DEBUG
    Serial.print("KWH: ");
    Serial.println(kwh);
    #endif
  } */
  if (data.startsWith("vo1=")) {
    voltage1 = data.substring(4).toInt();
    #ifdef DEBUG 
    Serial2.print("Voltage 1: ");
    Serial2.println(voltage1);
    #endif
  } 
  else if (data.startsWith("vo2=")) {
    #ifdef DEBUG 
    voltage2 = data.substring(4).toInt();
    Serial2.print("Voltage 2: ");
    Serial2.println(voltage2);
    #endif
  } 
  else if (data.startsWith("vo3=")) {
    voltage3 = data.substring(4).toInt();
    #ifdef DEBUG
    Serial2.print("Voltage 3: ");
    Serial2.println(voltage3);
    #endif
  } 
  else if (data.startsWith("cu1=")) {
    current1 = data.substring(4).toInt();
    #ifdef DEBUG
    Serial2.print("Current 1: ");
    Serial2.println(current1);
    #endif
  } 
  else if (data.startsWith("cu2=")) {
    current2 = data.substring(4).toInt();
    #ifdef DEBUG
    Serial2.print("Current 2: ");
    Serial2.println(current2);
    #endif
  } 
  else if (data.startsWith("cu3=")) {
    current3 = data.substring(4).toInt();
    #ifdef DEBUG
    Serial2.print("Current 3: ");
    Serial2.println(current3);
    #endif
  }
  else if (data.startsWith("tem=")) {
    current3 = data.substring(4).toInt();
    #ifdef DEBUG
    Serial2.print("Temperature : ");
    Serial2.println(temperature);
    #endif
  }
  else {
    Serial2.println("Unknown data type");
  }
}
