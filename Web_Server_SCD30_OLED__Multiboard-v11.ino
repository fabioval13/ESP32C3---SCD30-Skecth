#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#else
#include <WiFi.h>
#include <WebServer.h>
WebServer server(80);
#endif

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "SparkFun_SCD30_Arduino_Library.h"
#include <WiFiUdp.h>
#include <Preferences.h>

/* ============================================================
   CONFIGURAZIONE
   ============================================================ */
const char* ssid     = "IOT_1";
const char* password = "Parkside_no060997";

const unsigned long INTERVALLO_SENSOR  = 2000;
const unsigned long INTERVALLO_DISPLAY = 30000;

/* ============================================================
   VARIABILI GLOBALI
   ============================================================ */
/* valori PUBBLICI (display / web / seriale) */
float co2  = 0;
float temp = 0;
float hum  = 0;

/* valori LIVE (solo sensore ogni 2s) */
float co2_live  = 0;
float temp_live = 0;
float hum_live  = 0;

float tempOffset = 2.7;
bool  ascEnabled = true;

unsigned long lastSensor  = 0;
unsigned long lastDisplay = 0;

SCD30 airSensor;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
Preferences prefs;

/* ============================================================
   LED
   ============================================================ */
#define LED_BUILTIN 10   // ESP32-C3 Super Mini

/* ============================================================
   NTP
   ============================================================ */
WiFiUDP udp;
const char* ntpServer = "pool.ntp.org";
time_t ultimaOra = 0;

/* -------- NTP (VERSIONE STABILE) -------- */
time_t getNtpTime() {
  byte packetBuffer[48];
  memset(packetBuffer, 0, 48);
  packetBuffer[0] = 0b11100011;

  udp.beginPacket(ntpServer, 123);
  udp.write(packetBuffer, 48);
  udp.endPacket();

  delay(1000);
  if (udp.parsePacket() == 0) return ultimaOra;

  udp.read(packetBuffer, 48);

  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]);
  unsigned long secsSince1900 = (highWord << 16) | lowWord;

  const unsigned long seventyYears = 2208988800UL;
  ultimaOra = secsSince1900 - seventyYears + 3600; // UTC+1
  return ultimaOra;
}

/* -------- FORMAT ORA -------- */
String formatTime(time_t t) {
  unsigned long s = t % 86400;
  unsigned int h = s / 3600;
  s %= 3600;
  unsigned int m = s / 60;
  unsigned int sec = s % 60;

  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", h, m, sec);
  return String(buf);
}

/* ============================================================
   PAGINA MONITOR (HEAD MODIFICATO)
   ============================================================ */
String getHTML() {
  String html =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<title>Monitor Aria</title>"
    "<style>"
    "body{font-family:sans-serif;text-align:center;background:#222;color:white;}"
    ".val{font-size:40px;color:#00ff00;}"
    "a{color:#00ff00;text-decoration:none;}"
    "</style>"
    "<script>"
    "setInterval(function(){ location.reload(); }, 30000);"
    "</script>"
    "</head><body>";

  html += "<h1>Qualita' Aria</h1>";
  html += "<p>CO2: <span class='val'>" + String(co2, 0) + " ppm</span></p>";
  html += "<p>Temperatura: " + String(temp, 1) + " °C</p>";
  html += "<p>Umidita': " + String(hum, 1) + " %</p>";
  html += "<p>Ultima lettura: " + formatTime(ultimaOra) + "</p>";
  html += "<p><a href='/config'>Configurazione</a></p>";
  html += "</body></html>";

  return html;
}

/* ============================================================
   PAGINA CONFIGURAZIONE
   ============================================================ */
String getConfigHTML() {
  String html =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<title>Config SCD30</title>"
    "<style>"
    "body{font-family:sans-serif;background:#f4f4f4;padding:20px;}"
    ".box{max-width:400px;margin:auto;background:white;padding:20px;border-radius:8px;}"
    "label{font-weight:bold;}"
    "input{width:100%;padding:8px;margin-top:5px;}"
    "button{padding:10px;width:100%;background:#007bff;color:white;border:none;border-radius:5px;}"
    "</style></head><body><div class='box'>";

  html += "<h2>Configurazione SCD30</h2>";
  html += "<form method='POST' action='/save'>";
  html += "<label>Offset temperatura (°C)</label>";
  html += "<input type='number' step='0.1' name='offset' value='" + String(tempOffset, 1) + "'>";

  html += "<label><br><input type='checkbox' name='asc' ";
  if (ascEnabled) html += "checked";
  html += "> Calibrazione CO2 automatica (ASC)</label><br><br>";

  html += "<button type='submit'>Salva</button>";
  html += "</form><br>";
  html += "<a href='/'>← Torna al monitor</a>";
  html += "</div></body></html>";

  return html;
}

/* ============================================================
   DISPLAY OLED
   ============================================================ */
void aggiornaDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("IP: ");
  display.println(WiFi.localIP());

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print("CO2 ");
  display.print((int)co2);
  display.setTextSize(1);
  display.println("ppm");

  display.setCursor(0, 45);
  display.print("T: ");
  display.print(temp, 1);
  display.print("C");

  display.setCursor(70, 45);
  display.print("H: ");
  display.print(hum, 1);
  display.print("%");

  display.display();
}

/* ============================================================
   SETUP
   ============================================================ */
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_BUILTIN, OUTPUT);

  Wire.begin(8, 9);
  Wire.setClock(100000);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  if (!airSensor.begin()) while (1);

  prefs.begin("scd30", false);
  tempOffset = prefs.getFloat("t_offset", 2.7);
  ascEnabled = prefs.getBool("asc", true);

  airSensor.setMeasurementInterval(2);
  airSensor.setTemperatureOffset(tempOffset);
  airSensor.setAutoSelfCalibration(ascEnabled);
  airSensor.beginMeasuring();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  udp.begin(2390);

  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });
  server.on("/config", []() {
    server.send(200, "text/html", getConfigHTML());
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("offset")) {
      tempOffset = server.arg("offset").toFloat();
      if (tempOffset < 0 || tempOffset > 10) tempOffset = 2.7;
      prefs.putFloat("t_offset", tempOffset);
      airSensor.setTemperatureOffset(tempOffset);
    }

    ascEnabled = server.hasArg("asc");
    prefs.putBool("asc", ascEnabled);
    airSensor.setAutoSelfCalibration(ascEnabled);

    server.send(200, "text/html",
                "<html><body><h3>Salvato</h3><a href='/'>OK</a></body></html>");
  });

  server.begin();

  ultimaOra = getNtpTime();
  lastSensor  = millis();
  lastDisplay = millis();
}

/* ============================================================
   LOOP
   ============================================================ */
void loop() {
  server.handleClient();

  unsigned long now = millis();

  if (now - lastSensor >= INTERVALLO_SENSOR) {
    if (airSensor.dataAvailable()) {
      co2_live  = airSensor.getCO2();
      temp_live = airSensor.getTemperature() - tempOffset;
      hum_live  = airSensor.getHumidity();
    }
    lastSensor = now;
  }

  if (now - lastDisplay >= INTERVALLO_DISPLAY) {

    co2  = co2_live;
    temp = temp_live;
    hum  = hum_live;

    ultimaOra = getNtpTime();

    Serial.print("CO2=");
    Serial.print(co2);
    Serial.print(" ppm  T=");
    Serial.print(temp);
    Serial.print(" C  H=");
    Serial.println(hum);

    Serial.print("Ora NTP: ");
    Serial.println(formatTime(ultimaOra));

    aggiornaDisplay();
    lastDisplay = now;
  }
}
