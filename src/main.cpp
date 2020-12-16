
/* Biblioteki i klasy */
#include "A9Gmodule.h"
#include "configurationServer.h"

/* Hardware Serial baud speed rate */
#define SERIAL_BAUD 115200

/* Zmienne globalne */
A9Gmodule locator;

/* Funkcje lokalne */
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
    startServerConfiguration();
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

