#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Wi-Fi Access Point ayarları
const char *ssid = "Solar_Charger";
const char *password = "12345678";

// Captive Portal için DNS ayarları
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
WebServer server(80);


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
 /*  server.onNotFound([]() {
    server.send(200, "text/html", "<h1>Merhaba, ESP32 Captive Portal</h1><p>Bu sayfa evcharger.com olarak görünecek.</p>");
  }); */

  server.on("/", handleRoot); // Ana sayfa isteği için handleRoot işlevini kullan
  server.on("/number", handleNumber); // /number isteği için handleNumber işlevini kullan
  // Web sunucusunu başlatma
  server.begin();
  Serial.println("Web sunucusu başlatıldı");
}

void loop() {
  // DNS ve Web sunucusu isteklerini dinleme
  dnsServer.processNextRequest();
  server.handleClient();
}
