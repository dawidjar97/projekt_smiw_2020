#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "A9Gmodule.h" //biblioteka do osbługi modułu GSM/GPRS

#define SERIAL_BAUD 115200
#define HTTP_SERVER_PORT 80
#define DEFAULT_SSID "Lokalizator ESP12S"
#define DEFAULT_PASSWORD "12345678"



/* Global Variables */

AsyncWebServer server(HTTP_SERVER_PORT);  //Serwer do obsługi konfiguracji
A9Gmodule locator;
bool espReboot=0;
/*
bool espReboot = false; //flaga restartu urządzenia po wstępnej konfiguracji
bool config = 1;  //flaga konfiguracji urządzenia
uint8_t reconfigCounter = 0;  //licznik kolejnych rekonfiguracji sieci
*/

/* Memory Strings *//*
String writeAPIKey = "HRDDRV3GPRPSUPBB";
String channelID = "1048994";
String nrTel = "515303896";
String lastLocation = "empty";
String lastLocationTime = "empty";*/

/* Temp Memory Strings  *//*
String batteryStatus = "";
String dateTime = "";*/

/* Communication Strings *//*
String a9gAnswer = "";
String command = "";
uint8_t separator=33; //zmienna używana do separacji współrzędnych GPS*/

/* Local Functions */

void setup()
{
  SPIFFS.begin(); //Start obsługi pamięci

  #if DEBUG
    Serial.begin(SERIAL_BAUD);  //Start serial
  #endif
  
  if(isConfigurationCompleted(SPIFFS)) //Sprawdzenie czy plik konfiguracyjny istnieje
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

    WiFi.softAP(DEFAULT_SSID, DEFAULT_PASSWORD);//stworzenie punktu dostępowego

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
        //restart urządzenia po zakończeniu konfiguracji
        espReboot=1;
      }
    });

    server.serveStatic("/static/", SPIFFS, "/static/");//Załadowanie dodatkowych plików
    server.begin();//Start serwera
    #if DEBUG
      Serial.println("Waiting for configuration");
    #endif
  }
}

void loop()
{ 
  locator.executeTask();
  if(espReboot) //restart urządzenia po konfiguracji
  {
    delay(5000);
    server.end();
    WiFi.softAPdisconnect();
    ESP.restart();
  }
}

