#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include "configurator.h"
#include <SoftwareSerial.h>
#include <stdio.h>

#define INTERNAL_LED 2
#define RESET_BUTTON_PIN 5
#define SERIAL_BAUD 115200
#define A9_BAUD_RATE 115200
#define HTTP_SERVER_PORT 80
#define DEFAULT_SSID "Lokalizator ESP12S"
#define DEFAULT_PASSWORD "12345678"

#define A9G_PON     16  //ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF    15  //ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE    13  //ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP    2  //ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE


AsyncWebServer server(HTTP_SERVER_PORT);

bool espReboot = false;
bool buttonState = false;

String writeAPIKey = "HRDDRV3GPRPSUPBB";
String channelID = "1048994";
String nrTel = "515303896";
String lastLocation = "brak";
String GPS_position = "";
int separator=33;
String topic ="channels/" + String( channelID ) + "/publish/"+String(writeAPIKey);

SoftwareSerial swSer(14, 12, false);

int A9GPOWERON();
void A9GMQTTCONNECT();
String sendData(String command, const int timeout, boolean debug);


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

    topic ="channels/" + String( channelID ) + "/publish/"+String(writeAPIKey);

    swSer.begin(A9_BAUD_RATE);
    pinMode(A9G_PON, OUTPUT);//LOW LEVEL ACTIVE
    pinMode(A9G_POFF, OUTPUT);//HIGH LEVEL ACTIVE
    pinMode(A9G_LOWP, OUTPUT);//LOW LEVEL ACTIVE

    digitalWrite(A9G_PON, HIGH); 
    digitalWrite(A9G_POFF, LOW); 
    digitalWrite(A9G_LOWP, HIGH);

    Serial.println("After 2s A9G will POWER ON.");
    delay(2000);
    if(A9GPOWERON()==1)
    {
        Serial.println("A9G POWER ON.");
    }

    delay(5000);
    A9GMQTTCONNECT();
    delay(3000);
    sendData("AT+GPS=1",1000,DEBUG);
    delay(5000);

  } 
  else
  { //config not loaded
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
}

void loop()
{ 


  GPS_position=sendData("AT+LOCATION=2",1000,DEBUG);
  if( GPS_position.indexOf("OK") >= 0 )
  {
    String LG=GPS_position.substring(17,43);
    for(uint8_t i=0;i<GPS_position.length();i++)
    {
      if(GPS_position[i]==',')
      {
        separator=i;
        break;
      }
    }
    String payload = String("field1=" + String(GPS_position.substring(separator-9,separator)) + "&field2=" + String(GPS_position.substring(separator+1,separator+10)));
    Serial.println(payload);
    String command="AT+MQTTPUB=\""+ topic + "\""+ ","+"\""+ payload + "&status=MQTTPUBLISH" + "\""+",0,0,0";
    //Serial.println(command);
    //sendData("AT+MQTTPUB=\"channels/1048994/publish/HRDDRV3GPRPSUPBB\",\"field1=128&field2=64&status=MQTTPUBLISH\",0,0,0",1000,DEBUG);
    sendData(command,1000,DEBUG);
    /*delay(1000);
    if( GPS_position.indexOf("OK") >= 0 )
    {
      delay(50000);
    }
    else
    {
      sendData("AT+MQTTCONN=\"mqtt.thingspeak.com\",1883,\"1048994\",120,1",1000,DEBUG);
    }*/
    delay(50000);
  }
  delay(10000);

  if(espReboot) {
    delay(5000);
    server.end();
    WiFi.softAPdisconnect();
    ESP.restart();
  }
}
String sendData(String command, const int timeout, boolean debug)
{
    String response = "";    
    swSer.print(command+'\r'); 
    long int time = millis();   
    while( (time+timeout) > millis())
    {
      while(swSer.available())
      {       
        char c = swSer.read(); 
        response+=c;
      }  
    }    
    if(debug)
    {
      Serial.print(response);
    }    
    return response;
}

int A9GPOWERON()
{
  digitalWrite(A9G_PON, LOW);
  delay(3000);
  digitalWrite(A9G_PON, HIGH);
  delay(5000);
  String msg = String("");
  msg=sendData("AT",1000,DEBUG);
  if( msg.indexOf("OK") >= 0 )
  {
      Serial.println("GET OK");
      return 1;
  }
  else 
  {
      Serial.println("NOT GET OK");
      return 0;
  }
}

void A9GMQTTCONNECT()
{
  delay(4000);
  sendData("AT+CGATT=1",1000,DEBUG); //Attach GPRS
  delay(1000);
  sendData("AT+CIPMUX=0",1000,DEBUG); //enable single IP connection
  delay(1000);
  sendData("AT+CSTT=\"plus\",\"\",\"\"",1000,DEBUG);  //APN
  delay(1000);
  sendData("AT+CIICR",1000,DEBUG);  //start wireless connection
  delay(1000);
  sendData("AT+CIFSR",1000,DEBUG);  //IP
  delay(1000);
  sendData("AT+CGACT=1,1",1000,DEBUG);  //start
  delay(1000);
  sendData("AT+MQTTDISCONN",1000,DEBUG);  //disc
  delay(2000);
  String msg=sendData("AT+MQTTCONN=\"mqtt.thingspeak.com\",1883,\"1048994\",120,1",1000,DEBUG);
  delay(4000);
  if( msg.indexOf("OK") >= 0 )
  {
    Serial.println("A9G CONNECT to the ThingSpeak");
  }
 delay(2000);
 //sendData("AT+MQTTPUB=\"channels/1048994/publish/HRDDRV3GPRPSUPBB\",\"field1=128&field2=64&status=MQTTPUBLISH\",0,0,0",1000,DEBUG);
 }

