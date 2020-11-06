#include <Arduino.h>
#include <ESPAsyncWebServer.h>
/* A9G GSM/GPRS module library */
#include "A9Gmodule.h"

/* Hardware Serial baud speed rate */
#define SERIAL_BAUD 115200

/* WiFi and server defines */
#define HTTP_SERVER_PORT 80
#define DEFAULT_SSID "Lokalizator ESP12S"
#define DEFAULT_PASSWORD "12345678"



/* Global Variables */

AsyncWebServer server(HTTP_SERVER_PORT);  //Serwer do obsługi konfiguracji
A9Gmodule locator;
bool espReboot=0;

/* Local Functions */

void setup()
{

  /* Inicjalizacja obsługi pamięci */
  SPIFFS.begin(); 

  #if DEBUG
    /* Uruchomienie komunikacji UART */
    Serial.begin(SERIAL_BAUD);  
  #endif
  
  /* Sprawdzenie czy plik konfiguracyjny istnieje */
  if(isConfigurationCompleted(SPIFFS)) 
  {
    #if DEBUG
      Serial.println("Configuration loaded");
    #endif

    locator.loadData();

    WiFi.mode(WIFI_OFF);  //Wyłączenie WiFi
    server.end(); //Zatrzymanie pracy serwera
    locator.setup();//przygotowanie lokalizatora do pracy
  } 
  else  //Brak konfiguracji urządzenia
  {
    #if DEBUG
      Serial.println("Configuration not loaded");
    #endif

    /* Inicjalizacja punktu dostępowego */
    WiFi.softAP(DEFAULT_SSID, DEFAULT_PASSWORD);

    /* Zdarzenia serwera */
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

        request->send(SPIFFS, "/initial_config_ready.html");

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
}

void loop()
{ 
  locator.executeTask();
  /* Restart urządzenia po konfiguracji */
  if(espReboot)
  {
    delay(5000);
    server.end();
    WiFi.softAPdisconnect();
    ESP.restart();
  }
}

