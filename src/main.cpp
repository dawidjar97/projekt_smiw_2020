#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include "configurator.h"

#define INTERNAL_LED 2
#define RESET_BUTTON_PIN 5
#define SERIAL_BAUD 115200
#define HTTP_SERVER_PORT 80
#define DEFAULT_SSID "Lokalizator ESP12S"
#define DEFAULT_PASSWORD "12345678"

AsyncWebServer server(HTTP_SERVER_PORT);

bool espReboot = false;
bool buttonState = false;

String writeAPIKey = "HRDDRV3GPRPSUPBB";
String channelID = "1048994";
String nrTel = "515303896";
String lastLocation = "brak";
String GPS_position = "";

void setup()
{
  SPIFFS.begin();

  #if DEBUG
    Serial.begin(SERIAL_BAUD);
  #endif

  pinMode(RESET_BUTTON_PIN, INPUT);
  buttonState = digitalRead(RESET_BUTTON_PIN);

  #if DEBUG
    Serial.print("Stan przycisku: "); Serial.println(buttonState);
  #endif

  if (buttonState) //Usuniecie konfiguracji
  {
    deleteFile(SPIFFS, CONFIG_PATH);
    
    #if DEBUG
      Serial.println("Konfiguracja usunieta, restart systemu.");
    #endif
    pinMode(INTERNAL_LED, OUTPUT);
    digitalWrite(INTERNAL_LED, HIGH);
    delay(500);
    digitalWrite(INTERNAL_LED, LOW);
    ESP.restart();
  }
  
  if(isConfigurationCompleted(SPIFFS)) 
  {
    #if DEBUG
      Serial.println("Konfiguracja zaladowano");
    #endif

  

    if(!loadConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, GPS_position)) 
    {
      #if DEBUG
        Serial.println("Something went wrong loading config file. Erasing config file");
        delay(500);
      #endif

      deleteFile(SPIFFS, CONFIG_PATH);
      ESP.restart();
    }

    //config loaded

  } 
  else
  { //BUT
    #if DEBUG
      Serial.println("Nie zaladowano konfiguracji");
    #endif

    WiFi.softAP(DEFAULT_SSID, DEFAULT_PASSWORD);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) 
    {
      request->send(SPIFFS, "/initial_config_hello.html");
    });

    server.on("/initial_config.html", HTTP_GET, [](AsyncWebServerRequest *request) 
    {
      request->send(SPIFFS, "/initial_config.html");
    });

    server.on("/initial_config_ready.html", HTTP_POST, [](AsyncWebServerRequest *request) 
    {
      if(request->hasParam("nr-tel", true) && request->hasParam("write-API-Key", true) && request->hasParam("channel-ID", true)) {
        AsyncWebParameter* tel = request->getParam("nr-tel", true);
        AsyncWebParameter* apiKey = request->getParam("write-API-Key", true);
        AsyncWebParameter* chID = request->getParam("channel-ID", true);

        #if DEBUG
          Serial.print("nr-tel: "); Serial.println(tel->value().c_str());
          Serial.print("write-API-Key: "); Serial.println(apiKey->value().c_str());
          Serial.print("channel-ID: "); Serial.println(chID->value().c_str());
        #endif

        request->send(SPIFFS, "/initial_config_ready.html", String(), false, [&tel](const String& var) {
          if(var == "NAZWA_SIECI") 
            return tel->value();
          return String();
        });

        if(!saveConfiguration(SPIFFS, tel->value(), apiKey->value(), chID->value(), lastLocation)) {
          #if DEBUG
            Serial.println("Something went horribly wrong.");
            pinMode(INTERNAL_LED, OUTPUT);
            digitalWrite(INTERNAL_LED, HIGH);
          #endif
        }

        espReboot = true;
      }
    });
  }

  IPAddress IP = WiFi.softAPIP();
  #if DEBUG
    Serial.print("AP IP address: "); Serial.println(IP);
  #endif

  server.serveStatic("/static/", SPIFFS, "/static/");
  server.begin();
  buttonState=false;
}

void loop()
{ 


  delay(60000);

  if(espReboot) {
    delay(5000);
    server.end();
    WiFi.softAPdisconnect();
    ESP.restart();
  }
}

