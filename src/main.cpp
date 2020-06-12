#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SoftwareSerial.h>
#include <stdio.h>
#include "configurator.h"

#define INTERNAL_LED 2
#define RESET_BUTTON_PIN 0
#define SERIAL_BAUD 115200
#define A9_BAUD_RATE 115200
#define HTTP_SERVER_PORT 80
#define DEFAULT_SSID "Lokalizator ESP12S"
#define DEFAULT_PASSWORD "12345678"

#define A9G_PON     16  //ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF    15  //ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE    13  //ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP    2  //ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE

/* Global Variables */

AsyncWebServer server(HTTP_SERVER_PORT);
SoftwareSerial swSer(14, 12, false);

bool espReboot = false;
bool buttonState = false;
bool config = 1;
uint8_t reconfigCounter = 0;

/* Memory Strings */
String writeAPIKey = "HRDDRV3GPRPSUPBB";
String channelID = "1048994";
String nrTel = "515303896";
String lastLocation = "empty";
String lastLocationTime = "empty";

String batteryStatus = "";
String dateTime = "";

/* Communication Strings */
String a9gAnswer = "";
String command = "";
uint8_t separator=33;

/* Local Functions */
int A9GPOWERON();
void A9GMQTTCONNECT();
void a9gCommunication(String command, const int timeout);

void setup()
{
  SPIFFS.begin();

  #if DEBUG
    Serial.begin(SERIAL_BAUD);
  #endif

  pinMode(RESET_BUTTON_PIN, INPUT);
  buttonState = 0;//digitalRead(RESET_BUTTON_PIN);

  #if DEBUG
    Serial.print("Button state: "); Serial.println(buttonState);
  #endif

  if (buttonState) //Usuniecie konfiguracji
  {
    deleteFile(SPIFFS, CONFIG_PATH);
    #if DEBUG
      Serial.println("Configuration deleted, system restart");
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
      Serial.println("Configuration loaded");
    #endif

    if(!loadConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, lastLocation)) 
    {
      #if DEBUG
        Serial.println("Something went wrong loading config file. Erasing config file");
        delay(500);
      #endif  

      deleteFile(SPIFFS, CONFIG_PATH);
      ESP.restart();
    }

    //config loaded

    WiFi.mode(WIFI_OFF);
    server.end();

    swSer.begin(A9_BAUD_RATE, SWSERIAL_8N1, 14, 12, false, 128);

    pinMode(A9G_PON, OUTPUT);//LOW LEVEL ACTIVE
    pinMode(A9G_POFF, OUTPUT);//HIGH LEVEL ACTIVE
    pinMode(A9G_LOWP, OUTPUT);//LOW LEVEL ACTIVE

    digitalWrite(A9G_PON, HIGH); 
    digitalWrite(A9G_POFF, LOW); 
    digitalWrite(A9G_LOWP, HIGH);

    #if DEBUG
      Serial.println("After 2s A9G will POWER ON.");
    #endif
    delay(2000);
    if(A9GPOWERON()==1)
    {
      #if DEBUG
        Serial.println("A9G POWER ON.");
      #endif
    }

    /* SMS prep */
    a9gCommunication("AT+CSCS=\"GSM\"",1000);
    a9gCommunication("AT+CMGF=1",1000);

    A9GMQTTCONNECT();
    a9gCommunication("AT+GPS=1",1000);
  } 
  else
  { //config not loaded
    #if DEBUG
      Serial.println("Configuration not loaded");
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

        if(!saveConfiguration(SPIFFS, tel->value(), apiKey->value(), chID->value(), lastLocation)) 
        {
          #if DEBUG
            Serial.println("Something went wrong.");
            pinMode(INTERNAL_LED, OUTPUT);
            digitalWrite(INTERNAL_LED, HIGH);
          #endif
        }

        espReboot = true;
      }
    });
    IPAddress IP = WiFi.softAPIP();
    #if DEBUG
      Serial.print("AP IP address: "); Serial.println(IP);
    #endif

    server.serveStatic("/static/", SPIFFS, "/static/");
    server.begin();
  }
}

void loop()
{ 
  if(espReboot) 
  {
    delay(5000);
    server.end();
    WiFi.softAPdisconnect();
    ESP.restart();
  }
  a9gCommunication("AT+CBC?",1000);
  a9gCommunication("AT+CCLK?",1000);
  a9gCommunication("AT+LOCATION=2",1000);
  if( a9gAnswer.indexOf(",") >= 0 )
  {
    /*for(uint8_t i=0;i<a9gAnswer.length();i++)
    {
      if(a9gAnswer[i]==',')
      {
        separator=i;
        break;
      }
    }*/
    lastLocationTime=dateTime;
    separator= a9gAnswer.indexOf(",");
    //String payload = String("field1=" + String(a9gAnswer.substring(separator-9,separator)) + "&field2=" + String(a9gAnswer.substring(separator+1,separator+10)));
    lastLocation=(a9gAnswer.substring(separator-9,separator))+", "+(a9gAnswer.substring(separator+1,separator+10));
    if(!saveConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, lastLocation)) 
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
    command="AT+MQTTPUB=\"channels/" + channelID + "/publish/"+ writeAPIKey + "\""+ ","+"\"field1=" + (a9gAnswer.substring(separator-9,separator)) + "&field2=" + (a9gAnswer.substring(separator+1,separator+10)) + "&status=MQTTPUBLISH" + "\""+",0,0,0";
    a9gCommunication(command,5000);
    if( a9gAnswer.indexOf("OK") >= 0 )
    {
      #if DEBUG
        Serial.println("MQTT sent");
      #endif
      a9gCommunication("",240000);  //4minutes break
    }
    else
    {
      config=1;
      A9GMQTTCONNECT();
      a9gCommunication(command,5000);
    }
  }
  Serial.println("60 seconds break");
  a9gCommunication("",60000);
  Serial.println("End of loop");

}
void sendSms(String msg)
{
  swSer.print("AT+CMGS=\""+ nrTel + "\"\r");
  swSer.println(msg);
  swSer.write(26); 
}
void clearSms()
{
  swSer.println("AT+CPMS=\"SM\",\"ME\"");
  swSer.println("AT+CMGD=1,4");
}
void a9gCommunication(String command, const int timeout)
{
  a9gAnswer = "";
  if(command!="")  
    swSer.print(command+'\r');
  //delay(10);
  long int time = millis(); 
  long int condition = time+timeout;
  while( (condition) > millis())
  {
    while(swSer.available())
    {       
      char c = swSer.read(); 
      a9gAnswer+=c;
      if(a9gAnswer.length()==2)
      {
        //if(!config)
          condition=millis()+2000;
      }
    }  
  }    
  #if DEBUG
    Serial.print(a9gAnswer);
  #endif
  /* Checking if message is a SMS */
  if(a9gAnswer.indexOf("MESSAGE") >= 0)
  {
    #if DEBUG
    Serial.println("Received a SMS");
    #endif
    if(a9gAnswer.indexOf("Bateria") >= 0)
    {
      sendSms("Stan baterii: "+batteryStatus+"%"); 
    }
    else if(a9gAnswer.indexOf("Lokalizuj") >= 0)
    {
      sendSms("Ostatnia lokalizacja: "+lastLocation);
    }
    else if(a9gAnswer.indexOf("Info") >= 0)
    {
      sendSms("Ostatnia lokalizacja:\n "+lastLocation+"\n "+lastLocationTime+"\n "+"Bateria: "+batteryStatus+"%");
    }
  }
  else if(a9gAnswer.indexOf("CBC:") >= 0)
  {
    separator = a9gAnswer.indexOf("CBC:");
    batteryStatus=a9gAnswer.substring(separator+4,separator+11);
  }
  else if(a9gAnswer.indexOf("CCLK:") >= 0)
  {
    separator = a9gAnswer.indexOf("CCLK:");
    dateTime=a9gAnswer.substring(separator+7,separator+27);
  }
  else if(a9gAnswer.indexOf("SMSFULL") >= 0 )
  {
    clearSms();
  }
}

int A9GPOWERON()
{
  digitalWrite(A9G_PON, LOW);
  delay(3000);
  digitalWrite(A9G_PON, HIGH);
  delay(5000);
  a9gCommunication("AT",1000);
  if(a9gAnswer.indexOf("OK") >= 0 )
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
  a9gCommunication("AT+MQTTCONN=\"mqtt.thingspeak.com\",1883,\"1048994\",120,1",5000);  //connect to MQTT server
  if(a9gAnswer.indexOf("OK") >= 0 ) //check module answer
  {
    Serial.println("A9G CONNECTED to the ThingSpeak");
  }
  else
  {
    if(reconfigCounter++==5)
      {
        ESP.restart();
      }
    A9GMQTTCONNECT();
  }

  config=0;
  
}

