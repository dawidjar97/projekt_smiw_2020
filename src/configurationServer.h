#pragma once

#include "configurator.h" //bibloteka do obsługi ładowania plików konfiguracyjnych
#include <ESPAsyncWebServer.h>

/* WiFi and server defines */
#define HTTP_SERVER_PORT 80
#define DEFAULT_SSID "Lokalizator ESP12S"
#define DEFAULT_PASSWORD "L0k4liz@t0r#01"

AsyncWebServer server(HTTP_SERVER_PORT);  //Serwer do obsługi konfiguracji
bool espReboot=0;

void startServerConfiguration()
{
    /* Inicjalizacja punktu dostępowego */
    WiFi.softAP(DEFAULT_SSID, DEFAULT_PASSWORD);

    /* Zdarzenia serwera */
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) 
    {
      request->send(SPIFFS, "/config_start.html");
    });

    server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request) 
    {
      request->send(SPIFFS, "/config.html");
    });

    server.on("/config_end.html", HTTP_POST, [](AsyncWebServerRequest *request) 
    {
      if(request->hasParam("nr-tel", true) && request->hasParam("write-API-Key", true) && request->hasParam("channel-ID", true)) 
      {
        AsyncWebParameter* tel = request->getParam("nr-tel", true);
        AsyncWebParameter* apiKey = request->getParam("write-API-Key", true);
        AsyncWebParameter* chID = request->getParam("channel-ID", true);

        #if DEBUG
          Serial.print("nr-tel: "); Serial.println(tel->value().c_str());
          Serial.print("write-API-Key: "); Serial.println(apiKey->value().c_str());
          Serial.print("channel-ID: "); Serial.println(chID->value().c_str());
        #endif

        request->send(SPIFFS, "/config_end.html");

        if(!saveConfiguration(SPIFFS, tel->value(), apiKey->value(), chID->value(), "null", "null"))//Zapis konfiguracji do pamięci
        {
          #if DEBUG
            Serial.println("Something went wrong.");
            pinMode(INTERNAL_LED, OUTPUT);
            digitalWrite(INTERNAL_LED, HIGH);
          #endif
        }
        /* Restart urządzenia po zakończeniu konfiguracji */
        espReboot=1;
      }
    });
    /* Załadowanie dodatkowych plików */
    server.serveStatic("/static/", SPIFFS, "/static/");
    /* Start serwera WWW */
    server.begin();
    #if DEBUG
      Serial.println("Waiting for configuration");
    #endif
}