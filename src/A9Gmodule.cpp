#include "A9Gmodule.h"


void A9Gmodule::sendSms(String msg)  //funkcja do wysyłania wiadomości SMS
{
  serial->flush();
  serial->print("AT+CMGS=\""+ nrTel + "\"\r");
  serial->println(msg);
  serial->write(26); //znak końca wiadomości SMS
}
void A9Gmodule::clearSms()
{
  serial->println("AT+CPMS=\"ME\",\"SM\""); //ustawienie miejsca przechowywania wiadomości SMS
  serial->println("AT+CMGD=1,4"); //Usunięcie wszystkich wiadomości
}

void A9Gmodule::checkMessage()
{
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
    batteryLevel=a9gAnswer.substring(separator+8,separator+11);
    batteryLevel.trim();
    #if DEBUG
      Serial.print("String: ");
      Serial.println(a9gAnswer);
      Serial.print("batteryLevel: ");
      Serial.println(batteryLevel);
      Serial.print("batteryStatus: ");
      Serial.println(batteryStatus);
    #endif
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

void A9Gmodule::a9gCommunication(String command, const int timeout)  //Funkcja do komunikacji dwustronnej z modułem GPRS/GPS
{
  a9gAnswer = ""; //wyczyszczenie odpowiedzi
  if(command!="")  
    serial->print(command+'\r');  //wyslanie komendy
  long int time = millis();   //pobranie aktualnego czasu
  long int condition = time+timeout;  //ustalenie warunku wyjścia
  while( (condition) > millis())
  {
    while(serial->available())
    {       
      char c = serial->read();  //pobranie odpowiedzi
      a9gAnswer+=c;
      if(a9gAnswer.length()==2) //Jeżeli odebrano już 2 znaki
      {
        if(command=="")
          condition=millis()+1000;  //jeżeli to zwykła komenda po otrzymaniu wyniku, czekaj jeszcze 1s i wyjdź z pętli
      }
    }  
  }    
  #if DEBUG
    Serial.print(a9gAnswer);
  #endif
  checkMessage();
}

int A9Gmodule::A9GPOWERON()
{
  /* Sekwencja uruchomienia modułu */
  digitalWrite(A9G_PON, LOW);
  delay(3000);
  digitalWrite(A9G_PON, HIGH);
  delay(5000);
  a9gCommunication("AT",2000);  //wysłanie komendy AT do modułu
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

void A9Gmodule::A9GMQTTCONNECT()
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
  a9gCommunication("AT+CSCS=\"GSM\"",1000); //ustawienie kodowania
}

void A9Gmodule::executeTask()
{
    if(!config) //normalny tryb pracy
    {
        a9gCommunication("AT+CBC?",1000); //Stan baterii
        a9gCommunication("AT+CCLK?",1000);  //Aktualna data i czas
        a9gCommunication("AT+LOCATION=2",1000); //Lokalizacja
        if(a9gAnswer.indexOf(",") >= 0 && a9gAnswer.indexOf("GPS NOT FIX")==-1)//sprawdzenie czy zwrócono lokalizacje
        {
            lastLocationTime=dateTime;  //czas ostatniej lokalizacji
            separator=a9gAnswer.indexOf(","); //ustawienie zmiennej separującej współrzędne geograficzne
            lastLocation=(a9gAnswer.substring(separator-9,separator))+", "+(a9gAnswer.substring(separator+1,separator+10)); //separacja współrzędnych geograficznych i zapis do zmiennej
            if(!saveConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, lastLocation, lastLocationTime)) //zapis współrzędnych geograficznych do pamięci urządzenia
            {
                #if DEBUG
                  Serial.println("Przy zapisywaniu wystapil blad, urzadzenie zostanie zrestartowane");
                #endif
                pinMode(INTERNAL_LED, OUTPUT);
                digitalWrite(INTERNAL_LED, HIGH);
            }
            #if DEBUG
                Serial.println(lastLocation);
            #endif
            /* Komenda do przesyłu MQTT */
            command="AT+MQTTPUB=\"channels/" + channelID + "/publish/"+ writeAPIKey + "\""+ ","+
            "\"field1=" + (a9gAnswer.substring(separator-9,separator)) +
             "&field2=" + (a9gAnswer.substring(separator+1,separator+10)) + 
             "&field3=" + dateTime + "&status=MQTTPUBLISH" + "\""+",0,0,0";
            a9gCommunication(command,5000); //Wysłanie pakietu MQTT

            if( a9gAnswer.indexOf("OK") >= 0 )  //Jeżeli wysłano poprawnie
            {
                #if DEBUG
                  Serial.println("MQTT sent");
                  Serial.println("240 sekund oczekiwania na komunikacje");
                #endif
                a9gCommunication("",240000);  //4minuty oczekiwania na SMS
            }
            else  //Jeżeli wystąpił błąd
            {
                #if DEBUG
                  Serial.println("MQTT not sent");
                #endif
                config=1; //tryb konfiguracji
                A9GMQTTCONNECT(); //ponowne połączenie do serwera MQTT
                a9gCommunication(command,5000); //próba ponownego wysłania pakietu MQTT
            }
        }
        #if DEBUG
        Serial.println("60 sekund oczekiwania na komunikacje");
        #endif
        command="AT+MQTTPUB=\"channels/" + channelID + "/publish/"+ writeAPIKey + "\",\"field4=" + batteryLevel + "&status=MQTTPUBLISH\""+",0,0,0";
        a9gCommunication(command,5000); //Wysłanie pakietu MQTT
        a9gCommunication("",60000); //Minuta oczekiwania na SMS
        #if DEBUG
        Serial.println("Koniec zadania");
        #endif
    }
}
void A9Gmodule::setup()
{
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
void A9Gmodule::loadData()
{
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
    serial->begin(A9_BAUD_RATE, SWSERIAL_8N1, 14, 12, false, 128);
}

A9Gmodule::A9Gmodule()
{
    serial = new SoftwareSerial(14, 12, false);
}

A9Gmodule::~A9Gmodule()
{
}
