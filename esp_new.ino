#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <string.h>

const char *ssid = "Solar_Charger";
const char *password = "12345678";
String charge_sit = "Solar Şarj İstasyonu";
String amper_sit = "";

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer server(80);

int kwh = 50; 

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  Serial.println("Access Point Başlatıldı");
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP adresi: ");
  Serial.println(IP);

  // DNS yönlendirmesi (herhangi bir URL'yi IP adresine yönlendir)
  dnsServer.start(DNS_PORT, "*", IP);
  
  // Ana sayfa ve herhangi bir sayfa isteği için yönlendirme
  server.on("/", handleRoot); // Ana sayfa isteği için handleRoot işlevini kullan
  server.on("/status", handleStatus); // /number isteği için handleNumber işlevini kullan
  server.on("/number", handleNumber);
  server.on("/button1", handleButton1);
  server.on("/button2", handleButton2);
  server.on("/button3", handleButton3);
  server.on("/button4", handleButton4);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);

  // Web sunucusunu başlatma
  server.begin();
  Serial.println("Web sunucusu başlatıldı");
}

void loop() {
  // DNS ve Web sunucusu isteklerini dinleme
  dnsServer.processNextRequest();
  server.handleClient();
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
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin-top: 75px;
        }
        .number {
            font-size: 48px;
            color: #333;
            margin-bottom: 20px;
        }
        .button-container {
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-top: 30px;
        }
        .button, .control-button, .submit-button {
            padding: 15px 30px;
            font-size: 18px;
            cursor: pointer;
            border: none;
            border-radius: 5px;
            color: #fff;
        }
        .button {
            background-color: #0373fc;
        }
        .control-button {
            background-color: #2196F3;
        }
        .start-button {
            background-color: #4CAF50;
        }
        .stop-button {
            background-color: #f44336;
        }
        .status-container {
            margin-top: 30px;
        }
        input[type="text"] {
            font-size: 18px;
            padding: 10px;
            width: 80%;
            max-width: 400px;
            margin-bottom: 20px;
            border: 2px solid #ccc;
            border-radius: 5px;
            text-align: center;
        }
        input[type="text"]:disabled {
            background-color: #f9f9f9;
            color: #333;
        }
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
        function fetchNumber() {
            fetch('/number')
                .then(response => response.text())
                .then(data => {
                    document.getElementById('number').innerText = data;
                })
                .catch(error => console.error('Hata:', error));
        }
        setInterval(fetchNumber, 500);

        function fetchStatus() {
            fetch('/status')
                .then(response => response.text())
                .then(data => {
                    document.getElementById('statusInput').value = data;
                })
                .catch(error => console.error('Durum alma hatası:', error));
        }
        setInterval(fetchStatus, 1000);

        function sendCommand(command) {
            fetch('/' + command)
                .then(response => response.text())
                .then(data => {
                    console.log('Komut gönderildi:', command);
                })
                .catch(error => console.error('Komut gönderme hatası:', error));
        }
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
  server.send(200, "text/plain", charge_sit +" "+ amper_sit);
}

void handleButton1() {
  amper_sit = "10 A";
}

void handleButton2() {
  amper_sit = "16 A";
}

void handleButton3() {
  amper_sit = "24 A";
}

void handleButton4() {
  amper_sit = "32 A";
}

void handleStart() {
  charge_sit = "ÇALIŞIYOR";
}
void handleStop() {
  charge_sit = "ÇALIŞMIYOR";
}
