#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <Wire.h>
#include <AM2320.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include <FS.h>

ESP8266WebServer server(80);
AM2320 sensor(&Wire);

const char *ssid = "SSID";
const char *password = "PASSWORD";

String temperature;
String humidity;

String formattedTime;
String formattedDate;

unsigned long previousTime = 0;
time_t epochTime;

bool logging = false;
unsigned int interval = 10; // segundos

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void handleRoot();
void handleNotFound();
void handleLog();
void handleStart();
void handleStop();

void logSensor();
void readSensor();

void tick();
bool checkTime();

void handleLogging();

void setup() {
  SPIFFS.begin();
  
  Serial.begin(115200);
  Serial.println();

  pinMode(BUILTIN_LED, OUTPUT);

  WiFi.begin(ssid, password);

  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Conectado a: ");
  Serial.println(WiFi.SSID());
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/log", handleLog);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);

  server.on("/handleLogging", HTTP_POST, handleLogging);
  
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado");

  timeClient.begin();
  timeClient.setTimeOffset(-18000); // GMT-5 (-5*3600 seconds) Colombia
  tick();

  Wire.begin();
  digitalWrite(BUILTIN_LED, HIGH); // LOW is ON
}

void loop(void) {
  server.handleClient();

  if (logging && checkTime()) {
    logSensor();
  }
}

bool checkTime() {
  tick();

  if (previousTime == 0 || epochTime - previousTime > interval) {
    previousTime = epochTime;
    return true;
  } else {
    return false;
  }
}

void logSensor() {
  readSensor();
  
  String fileName = String("/" + formattedDate + ".txt");
  File logFile = SPIFFS.open(fileName, "a");
  logFile.println(String(formattedDate + ", " + formattedTime + ", " + temperature + ", " + humidity));
  logFile.close();
}

void readSensor() {
  digitalWrite(BUILTIN_LED, LOW);
  
  switch(sensor.Read()) {
    case 2:
      Serial.println(F(" Error CRC"));
      break;
    case 1:
      Serial.println(F(" Sensor desconectado"));
      break;
    case 0:
      temperature = String(sensor.cTemp, 1);
      humidity = String(sensor.Humidity, 1);
      Serial.print(F(" Humedad = "));
      Serial.println(String(humidity + "%"));
      Serial.print(F(" Temperatura = "));
      Serial.println(String(temperature + "*C"));
      Serial.println();
      break;
  }
  
  digitalWrite(BUILTIN_LED, HIGH);
}

void tick() {
  timeClient.update();
  
  epochTime = timeClient.getEpochTime();
  formattedTime = timeClient.getFormattedTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);

  int currentYear = ptm->tm_year+1900;
  int currentMonth = ptm->tm_mon+1;
  int monthDay = ptm->tm_mday;
  
  formattedDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
}

// URL handles

void handleStop() {
  logging = false;
  
  String message = "Log detenido";
  String html = "<p>" + message + "</p><p><a href=\"/\">Inicio</a></p>";
  server.send(200, "text/html", html);
}

void handleStart() {
  String html = "<form action= \"/handleLogging\" method=\"POST\"><input type=\"text\" name=\"intervalText\" placeholder=\"Intervalo en minutos\"></br><input type=\"submit\" value=\"Iniciar\"></form>";
  server.send(200, "text/html", html);
}

void handleLogging() {
  String intervalText = server.arg("intervalText");
  
  if(intervalText) {
    Serial.println("Hay un argumento POST");
    int intervalConverted = intervalText.toInt()*60;
    Serial.println("Intervalo convertido");
    interval = intervalConverted;
    logging = true;
    
    String html = "<p>Iniciando log con intervalo de " + intervalText + " minutos</p><p><a href=\"/log\">Ver log</a></p>";
    server.send(200, "text/html", html);
  } else {
    Serial.println("Algo esta mal con el argumento");
    server.sendHeader("Location","/");
    server.send(303);
  }
}

void handleRoot() {
  tick();
  readSensor();
  
  String formattedData = String(formattedDate + ", " + formattedTime + ", Temperatura: " + temperature + " C," + " Humedad: " + humidity + "%");
  String html = "<p>" + formattedData + "</p><p><a href=\"/start\">Iniciar log</a></p><p><a href=\"/stop\">Detener log</a></p><p><a href=\"/log\">Ver log</a></p>";
  server.send(200, "text/html", html);
}

void handleLog() {
  tick();
  
  String fileName = String("/" + formattedDate + ".txt");
  File file = SPIFFS.open(fileName, "r");
  server.streamFile(file, "text/plain");
  file.close();
}

void handleNotFound() {
  String html = "404: No encontrado";
  server.send(404, "text/plain", html);
}
