#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SoftwareSerial.h>
#include <stdio.h>
#include "configurator.h" //bibloteka do obsługi ładowania plików konfiguracyjnych

#define INTERNAL_LED 2
#define SERIAL_BAUD 115200
#define A9_BAUD_RATE 9600
#define HTTP_SERVER_PORT 80
#define DEFAULT_SSID "Lokalizator ESP12S"
#define DEFAULT_PASSWORD "12345678"

#define A9G_PON     16  //ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF    15  //ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE    13  //ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP    2  //ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE

/* Global Variables */

AsyncWebServer server(HTTP_SERVER_PORT);  //Serwer do obsługi konfiguracji
SoftwareSerial swSer(14, 12, false);  //SoftwareSerial do obsługi komunikacji między modułem GSM/GPS a mikrokontrolerem

bool espReboot = false; //flaga restartu urządzenia po wstępnej konfiguracji
bool config = 1;  //flaga konfiguracji urządzenia
uint8_t reconfigCounter = 0;  //licznik kolejnych rekonfiguracji 

/* Memory Strings */
String writeAPIKey = "HRDDRV3GPRPSUPBB";
String channelID = "1048994";
String nrTel = "515303896";
String lastLocation = "empty";
String lastLocationTime = "empty";

/* Temp Memory Strings  */
String batteryStatus = "";
String dateTime = "";

/* Communication Strings */
String a9gAnswer = "";
String command = "";
uint8_t separator=33; //zmienna używana do separacji współrzędnych GPS

/* Local Functions */
int A9GPOWERON(); //Funkcja włączająca moduł GPRS/GPS
void A9GMQTTCONNECT();  //Funkcja inicjująca połączenie do serwera MQTT
void a9gCommunication(String command, const int timeout); //Funkcja do komunikacji dwustronnej z modułem GPRS/GPS
void clearSms();  //Funkcja do usuwania wiadomości tekstowych z pamięci modułu GPRS/GPS

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

    if(!loadConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, lastLocation, lastLocationTime)) //Załadowanie zawartości pliku konfiguracyjnego do odpowiednich zmiennych
    {
      #if DEBUG
        Serial.println("Something went wrong loading config file. Erasing config file");
        delay(500);
      #endif  
      //Zabezpieczenie
      deleteFile(SPIFFS, CONFIG_PATH);  //usunięcie konfiguracji
      ESP.restart();  //restart urządzenia
    }
    
    swSer.begin(A9_BAUD_RATE, SWSERIAL_8N1, 14, 12, false, 128);
    WiFi.mode(WIFI_OFF);  //Wyłączenie WiFi
    server.end(); //Zatrzymanie pracy serwera
    
    pinMode(A9G_PON, OUTPUT);//LOW LEVEL ACTIVE
    pinMode(A9G_POFF, OUTPUT);//HIGH LEVEL ACTIVE
    pinMode(A9G_LOWP, OUTPUT);//LOW LEVEL ACTIVE

    //Ustawienie pinów sterujących modułu GPRS/GSM
    digitalWrite(A9G_PON, HIGH);
    digitalWrite(A9G_POFF, LOW);
    digitalWrite(A9G_LOWP, HIGH);

    #if DEBUG
      Serial.println("After 2s A9G will POWER ON.");
    #endif
    delay(2000);  //2 sekundowe opóźnienie
    if(A9GPOWERON()==1)
    {
      #if DEBUG
        Serial.println("A9G POWER ON.");
      #endif
    }

    /* SMS prep */
    a9gCommunication("AT+CSCS=\"GSM\"",1000); //ustawienie kodowania
    a9gCommunication("AT+CMGF=1",1000); //ustawienie trybu tekstowego

    A9GMQTTCONNECT(); //połaczenie do serwera MQTT
    clearSms(); //wyczyszczenie SMSów
    a9gCommunication("AT+GPS=1",1000);
  } 
  else  //config not loaded
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

        request->send(SPIFFS, "/initial_config_ready.html", String(), false, [&tel](const String& var) 
        {
          if(var == "NAZWA_SIECI") 
            return tel->value();
          return String();
        });

        if(!saveConfiguration(SPIFFS, tel->value(), apiKey->value(), chID->value(), lastLocation, lastLocationTime))//Zapis konfiguracji do pamięci
        {
          #if DEBUG
            Serial.println("Something went wrong.");
            pinMode(INTERNAL_LED, OUTPUT);
            digitalWrite(INTERNAL_LED, HIGH);
          #endif
        }

        espReboot = true;//ustawienie flagi restartu
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
  if(espReboot) //restart urządzenia po konfiguracji
  {
    delay(5000);
    server.end();
    WiFi.softAPdisconnect();
    ESP.restart();
  }
  if(!config) //normalny tryb pracy
  {
    a9gCommunication("AT+CBC?",1000); //Stan baterii
    a9gCommunication("AT+CCLK?",1000);  //Aktualna data i czas
    a9gCommunication("AT+LOCATION=2",1000); //Lokalizacja
    if(a9gAnswer.indexOf(",") >= 0 && a9gAnswer.indexOf("GPS NOT FIX")==-1)//sprawdzenie czy zwrócono lokalizacje
    {
      lastLocationTime=dateTime;  //czas ostatniej lokalizacji
      separator=a9gAnswer.indexOf(",");
      lastLocation=(a9gAnswer.substring(separator-9,separator))+", "+(a9gAnswer.substring(separator+1,separator+10));//separacja współrzędnych i zapis do zmiennej
      if(!saveConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, lastLocation, lastLocationTime)) //zapis do pamięci
      {
        #if DEBUG
          Serial.println("Something went wrong.");
          pinMode(INTERNAL_LED, OUTPUT);
          digitalWrite(INTERNAL_LED, HIGH);
        #endif
      }
      #if DEBUG
        Serial.println(lastLocation);
      #endif
      /* Komenda do przesyłu MQTT */
      command="AT+MQTTPUB=\"channels/" + channelID + "/publish/"+ writeAPIKey + "\""+ ","+"\"field1=" + (a9gAnswer.substring(separator-9,separator)) + "&field2=" + (a9gAnswer.substring(separator+1,separator+10)) + "&status=MQTTPUBLISH" + "\""+",0,0,0";
      a9gCommunication(command,5000);//Wysłanie pakietu MQTT
      if( a9gAnswer.indexOf("OK") >= 0 )  //Jeżeli wysłano poprawnie
      {
        #if DEBUG
          Serial.println("MQTT sent");
        #endif
        a9gCommunication("",240000);  //4minuty oczekiwania na SMS
      }
      else  //Jeżeli wystąpił błąd
      {
        config=1; //tryb konfiguracji
        A9GMQTTCONNECT(); //ponowne połączenie do serwera MQTT
        a9gCommunication(command,5000); //próba ponownego wysłania pakietu MQTT
      }
    }
    #if DEBUG
      Serial.println("60 seconds break");
    #endif
    a9gCommunication("",60000); //Minuta oczekiwania na SMS
    #if DEBUG
      Serial.println("End of loop");
    #endif
  }
}
void sendSms(String msg)  //funkcja do wysyłania wiadomości SMS
{
  swSer.flush();
  swSer.print("AT+CMGS=\""+ nrTel + "\"\r");
  swSer.println(msg);
  swSer.write(26); //znak końca wiadomości SMS
}
void clearSms()
{
  swSer.println("AT+CPMS=\"ME\",\"SM\""); //ustawienie miejsca przechowywania wiadomości SMS
  swSer.println("AT+CMGD=1,4"); //Usunięcie wszystkich wiadomości
}
void a9gCommunication(String command, const int timeout)  //Funkcja do komunikacji dwustronnej z modułem GPRS/GPS
{
  a9gAnswer = ""; //wyczyszczenie odpowiedzi
  if(command!="")  
    swSer.print(command+'\r');  //wyslanie komendy
  long int time = millis();   //pobranie aktualnego czasu
  long int condition = time+timeout;  //ustalenie warunku wyjścia
  while( (condition) > millis())
  {
    while(swSer.available())
    {       
      char c = swSer.read();  //pobranie odpowiedzi
      a9gAnswer+=c;
      if(a9gAnswer.length()==2) //Jeżeli odebrano już 2 znaki
      {
        if(!config)
          condition=millis()+1000;  //jeżeli to zwykła komenda po otrzymaniu wyniku, czekaj jeszcze 1s i wyjdź z pętli
      }
    }  
  }    
  #if DEBUG
    Serial.print(a9gAnswer);
  #endif
  /* Sprawdzanie czy odpowiedź jest SMSem */
  if(a9gAnswer.indexOf("MESSAGE") >= 0)
  {
    #if DEBUG
    Serial.println("Received a SMS");
    #endif
    if(a9gAnswer.indexOf("Bateria") >= 0) 
    {
      sendSms("Stan baterii: "+batteryStatus+"%"); //wysłanie stanu baterii
    }
    else if(a9gAnswer.indexOf("Lokalizuj") >= 0)  
    {
      sendSms("Ostatnia lokalizacja: "+lastLocation); //wysłanie ostatniej lokalizacji
    }
    else if(a9gAnswer.indexOf("Info") >= 0)
    {
      sendSms("Ostatnia lokalizacja:\n "+lastLocation+"\n "+lastLocationTime+"\n "+"Bateria: "+batteryStatus+"%");  //wysłanie wszystkich informacji
    }
    else if(a9gAnswer.indexOf("?R3ST4RT!.") >= 0 )  //reset ustawień urządzenie
    {
      deleteFile(SPIFFS, CONFIG_PATH);
      #if DEBUG
        Serial.println("Configuration deleted, system restart");
      #endif
      pinMode(INTERNAL_LED, OUTPUT);
      digitalWrite(INTERNAL_LED, HIGH); //sygnalizacja diodą LED
      delay(500);
      digitalWrite(INTERNAL_LED, LOW);
      ESP.restart();  //restart urządzenia
    }
  }
  else if(a9gAnswer.indexOf("CBC:") >= 0) //pobranie aktualnego stanu baterii
  {
    separator = a9gAnswer.indexOf("CBC:");
    batteryStatus=a9gAnswer.substring(separator+4,separator+11);
  }
  else if(a9gAnswer.indexOf("CCLK:") >= 0)  //pobranie aktualnej daty i czasu
  {
    separator = a9gAnswer.indexOf("CCLK:");
    dateTime=a9gAnswer.substring(separator+7,separator+27);
  }
  else if(a9gAnswer.indexOf("SMSFULL") >= 0 ) //jeżeli pamięc na sms pełna
  {
    clearSms(); //wyczyść SMS
  }
  
}

int A9GPOWERON()
{
  /* Sekwencja uruchomienia modułu */
  digitalWrite(A9G_PON, LOW);
  delay(3000);
  digitalWrite(A9G_PON, HIGH);
  delay(5000);
  a9gCommunication("AT",1000);  //wysłanie komendy AT do modułu
  if(a9gAnswer.indexOf("OK") >= 0 ) //sprawdzenie odpowiedzi
  {
    #if DEBUG
      Serial.println("GET OK");
    #endif
    return 1;
  }
  else 
  {
    #if DEBUG
      Serial.println("NOT GET OK");
    #endif
    return 0;
  }
}

void A9GMQTTCONNECT()
{
  /* GPRS */
  a9gCommunication("AT+CGATT=1",1000); //Attach GPRS
  a9gCommunication("AT+CIPMUX=0",1000); //enable single IP connection
  a9gCommunication("AT+CSTT=\"plus\",\"\",\"\"",1000);  //APN
  a9gCommunication("AT+CIICR",1000);  //start wireless connection
  a9gCommunication("AT+CIFSR",1000);  //IP
  a9gCommunication("AT+CGACT=1,1",1000);  //start
  a9gCommunication("AT+MQTTDISCONN",2000);  //disconnect from MQTT server
  a9gCommunication("AT+MQTTCONN=\"mqtt.thingspeak.com\",1883,\"1048994\",120,1",5000);  //połączenie do serwera MQTT
  if(a9gAnswer.indexOf("OK") >= 0 ) //sprawdzenie odpowiedzi modułu
  {
    Serial.println("A9G CONNECTED to the ThingSpeak");
    reconfigCounter=0;  //restart flagi rekonfiguracji
  }
  else
  {
    if(reconfigCounter++==5)
      {
        ESP.restart();  //restart urządzenie po 5 kolejnych rekonfiguracjach
      }
    A9GMQTTCONNECT(); //ponowne wywołanie funkcji
  }
  config=0; //restart flagi konfiguracji
}

