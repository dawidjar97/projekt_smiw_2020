#include "A9Gmodule.h"

/* Metoda do wysyłania wiadomości SMS */
void A9Gmodule::sendSms(String msg)
{
  serial->flush();
  serial->print("WAKE UP\r");
  serial->print("AT+CMGS=\"" + nrTel + "\"\r");
  serial->println(msg);
  serial->write(26); //znak końca wiadomości SMS
}

/* Metoda do usuwania wiadomości tekstowych z pamięci modułu GPRS/GPS*/
void A9Gmodule::clearSms()
{
  /* Ustawienie miejsca przechowywania wiadomości SMS */
  serial->println("AT+CPMS=\"ME\",\"SM\"");
  /* Usunięcie wszystkich wiadomości */
  serial->println("AT+CMGD=1,4");
}

void A9Gmodule::checkMessage()
{
  /* Sprawdzanie czy odpowiedź jest SMSem lub innym zapytaniem */
  if(a9gAnswer.indexOf("MESSAGE") >= 0)
  {
    #if DEBUG
    Serial.println("Received a SMS");
    #endif
    if(a9gAnswer.indexOf("Info") >= 0)
    {
      /* Wysłanie ostatniej lokalizacji i informacji o stanie baterii */
      sendSms("Ostatnia lokalizacja:\n " + lastLocation + "\n " + lastLocationTime + "\n " + "Bateria: " + batteryLevel + "%");
    }
    else if(a9gAnswer.indexOf("ChangeTaskPeriod:") >= 0)
    {
      changeTaskPeriod();
    }
    else if(a9gAnswer.indexOf("R3ST4RT") >= 0 )  //wyłączenie modułu i ponowne uruchomienie
    {
      restart();
    }
    else if(a9gAnswer.indexOf("?R3S3T!.") >= 0 )  //reset ustawień urządzenie
    {
      reset();
    }
  }
  else if(a9gAnswer.indexOf("CBC:") >= 0)
  {
    batteryUpdate();
  }
  else if(a9gAnswer.indexOf("CCLK:") >= 0)  //pobranie aktualnej daty i czasu
  {
    separator = a9gAnswer.indexOf("CCLK:");
    dateTime = a9gAnswer.substring(separator + 7, separator + 27);
  }
  else if(a9gAnswer.indexOf("SMSFULL") >= 0 ) //jeżeli pamięć na sms pełna
  {
    clearSms(); //wyczyść SMS
  }
}

int A9Gmodule::A9GPOWERON()
{
  /* Sekwencja uruchomienia modułu */
  digitalWrite(A9G_PON, LOW);
  delay(3000);
  digitalWrite(A9G_PON, HIGH);
  delay(5000);
  /* Wysłanie komendy AT do modułu */
  a9gCommunication("AT", 2000);
  /* Sprawdzenie odpowiedzi */
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

void A9Gmodule::setup()
{
  /* Rozpoczęcie komunikacji między modułem GSM/GPS a ESP8266 */
  serial->begin(A9_BAUD_RATE, SWSERIAL_8N1, 14, 12, false, 128);

  /* Ustawienie pinów sterujących modułu GPRS/GSM */
  pinMode(A9G_PON, OUTPUT);
  pinMode(A9G_POFF, OUTPUT); 
  pinMode(A9G_LOWP, OUTPUT); 

  digitalWrite(A9G_PON, HIGH);
  digitalWrite(A9G_POFF, LOW);
  digitalWrite(A9G_LOWP, HIGH);

  #if DEBUG
    Serial.println("A9G zostanie wlaczony");
  #endif

  /* 2 sekundowe opóźnienie */
  delay(2000);  
  /* Sprawdzenie poprawności uruchomienia modułu GSM/GPS */
  if(A9GPOWERON() == 1)
  {
    #if DEBUG
      Serial.println("A9G wlaczony");
    #endif
  }
  delay(1000);
  /* Przygotowanie modułu do odbioru i wysyłania wiadmości tekstowych */
  a9gCommunication("AT+CSCS=\"GSM\"", 1000); //ustawienie kodowania
  a9gCommunication("WAKE UP", 0);
  a9gCommunication("AT+SLEEP=0", 1000);
  a9gCommunication("AT+CMGF=1", 1000); //ustawienie trybu tekstowego
  
  /* Połączenie do serwera MQTT */
  A9GMQTTCONNECT();
  /* Czyszczenie pamięci wiadomości tekstowych modułu */
  clearSms();
  /* Włączenie modułu GPS */
  a9gCommunication("AT+GPS=1", 1000);
  a9gCommunication("AT+GPSLP=1", 1000);
  a9gCommunication("AT+SLEEP=2", 1000);
}

void A9Gmodule::reset()
{
  deleteFile(SPIFFS, CONFIG_PATH);
      #if DEBUG
        Serial.println("Konfiguracja usunieta, restart urzadzenia");
      #endif
  pinMode(INTERNAL_LED, OUTPUT);
  /* Sygnalizacja diodą LED */
  digitalWrite(INTERNAL_LED, HIGH);
  delay(500);
  digitalWrite(INTERNAL_LED, LOW);
  /* Restart urządzenie */
  ESP.restart(); 
}

void A9Gmodule::restart()
{
  /* Wyłączenie modułu GSM/GPS */
        digitalWrite(A9G_POFF, HIGH);
        delay(3000);
        digitalWrite(A9G_POFF, LOW);
        delay(5000);
  /* Restart urządzenie */
        ESP.restart();  
}

void A9Gmodule::a9gCommunication(String command, const int timeout)  //Funkcja do komunikacji dwustronnej z modułem GPRS/GPS
{
  /* Wyczyszczenie odpowiedzi */
  a9gAnswer = ""; 
  /* Wyslanie komendy (jeżeli takowa została przekazana) */
  if(command != "")
  {
    serial->print(command + '\r'); 
  }
     
  /* Pobranie aktualnego czasu */
  long int time = millis();
  /* Ustalenie warunku wyjścia */
  long int condition = time + timeout;
  /* Pętla while */
  while( (condition) > millis())
  {
    while(serial->available())
    {       
      /* Pobranie odpowiedzi */
      char sign = serial->read();  
      a9gAnswer += sign;
      /* Jeżeli odebrano już 2 znaki aktualizacja warunku wyjścia */
      if(a9gAnswer.length() == 2) 
      {
        /* Jeżeli to oczekiwanie na wiadomość po otrzymaniu wyniku, czekaj jeszcze 1s i wyjdź z pętli */
        if(command == "")
          condition = millis() + 1000;
      }
    }  
  }    
  #if DEBUG
    Serial.print(a9gAnswer);
  #endif
  /* Sprawdzenie wiadomości zwrotnej */
  checkMessage();
}

void A9Gmodule::A9GMQTTCONNECT()
{
  a9gCommunication("WAKE UP", 0);
  /* GPRS */
  a9gCommunication("AT+CGATT=1", 1000); //Włączenie GPRS
  a9gCommunication("AT+CIPMUX=0", 1000); //Tryb pracy dla jednego połączenia
  a9gCommunication("AT+CSTT=\"plus\",\"\",\"\"", 1000);  //Ustawienia APN
  a9gCommunication("AT+CIICR", 1000);  //Rozpoczęcie połączenie GPRS
  a9gCommunication("AT+CIFSR", 1000);  //Wyświetlenie adresu IP
  a9gCommunication("AT+CGACT=1,1", 1000);  //Aktywacja PDP
  a9gCommunication("AT+MQTTDISCONN", 2000);  //Rozłączenie serwera MQTT
  /* Połączenie do serwera MQTT */
  a9gCommunication("AT+MQTTCONN=\"mqtt.thingspeak.com\",1883,\"1048994\",120,1", 5000);
  /* Sprawdzenie odpowiedzi modułu */
  if(a9gAnswer.indexOf("OK") >= 0 )
  {
    Serial.println("A9G polaczony do ThingSpeak");
    reconfigCounter = 0;  //restart flagi rekonfiguracji
  }
  else
  {
    /* Zabepieczenie przed nieskończoną konfiguracją*/
    if(reconfigCounter++ == 5)
      {
        /* Wyłączenie modułu GSM/GPS i ponowne uruchomienie urządzenia */
        restart();
      }
    /* Ponowne wywołanie funkcji */
    A9GMQTTCONNECT();
  }
  /* Restart flagi konfiguracji */
  configFlag = 0;
  /* Ustawienie kodowania wiadomości tekstowych */
  a9gCommunication("AT+CSCS=\"GSM\"",1000);
}

void A9Gmodule::executeTask()
{
  /* Sprawdzenie trybu pracy */
  if(!configFlag)
  {
    /* Aktualizacja stanu baterii */
    a9gCommunication("WAKE UP", 0);
    a9gCommunication("AT+CBC?", 1000); //Zapytanie o stan baterii
    /* Aktualizacja daty i godziny */
    a9gCommunication("WAKE UP", 0);
    a9gCommunication("AT+CCLK?", 1000); //Zapytanie o aktualną datę i godzinę
    /* Aktualizacja lokalizacji */
    a9gCommunication("WAKE UP", 0);
    a9gCommunication("AT+LOCATION=2", 1000);  //Zapytanie o aktualną lokalizację
    /* Sprawdzenie czy zwrócono lokalizację */
    if(a9gAnswer.indexOf(",") >= 0 && a9gAnswer.indexOf("GPS NOT FIX") == -1)
    {
      /* Przygotowanie lokalizacji do wysłania i zapis do pamięci urządzenia */
      prepareAndSaveLocation();
      /* Wysłanie pakietu MQTT */
      a9gCommunication("WAKE UP", 0);
      a9gCommunication(command, 5000);
      /* Sprawdzenie poprawności wysłania pakietu MQTT */
      if( a9gAnswer.indexOf("OK") >= 0 )
      {
        /* Jeżeli pakiet wysłano poprawnie */
          #if DEBUG
            Serial.println("MQTT wyslane");
          #endif
      }
      else
      {
        /* Jeżeli wystąpił błąd przy wysyłaniu pakietu */
        #if DEBUG
          Serial.println("MQTT niewyslane");
        #endif
        /* Rekonfiguracja modułu GSM i połączenia z serwerem MQTT */
        configFlag = 1;
        A9GMQTTCONNECT();
        /* Ponowna próba wysłania pakietu MQTT z danymi */
        a9gCommunication("WAKE UP", 0);
        a9gCommunication(command, 5000);
      }
    }
    else
    {
      /* Wysłanie pakietu MQTT z aktualnym stanem baterii */
      command = "AT+MQTTPUB=\"channels/" + channelID + "/publish/"+ writeAPIKey + "\",\"field4=" + batteryLevel + "&status=MQTTPUBLISH\""+",0,0,0";
      a9gCommunication("WAKE UP", 0);
      a9gCommunication(command, 5000); //Wysłanie pakietu MQTT
    }
    #if DEBUG
      Serial.print(executeTaskTimePeriod/1000);
      Serial.println(" oczekiwania na komunikacje");
    #endif
    a9gCommunication("", executeTaskTimePeriod); //Czas oczekiwania na SMS
    
    #if DEBUG
      Serial.println("Koniec zadania");
    #endif
  }
}

void A9Gmodule::loadData()
{
    if(!loadConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, lastLocation, lastLocationTime)) //Załadowanie zawartości pliku konfiguracyjnego do odpowiednich zmiennych
    {
      #if DEBUG
        Serial.println("Blad odczytu pliku konfigracyjnego. Czyszczenie konfiguracji...");
        delay(500);
      #endif  
      /* Zabezpieczenie */
      deleteFile(SPIFFS, CONFIG_PATH);  //usunięcie konfiguracji
      ESP.restart();  //restart urządzenia
    }
}

void A9Gmodule::batteryUpdate()
{
  /* Pobranie aktualnego stanu baterii */
  separator = a9gAnswer.indexOf("CBC:");
  batteryLevel = a9gAnswer.substring(separator + 8, separator + 11);
  batteryLevel.trim();
  #if DEBUG
    Serial.print("batteryLevel: ");
    Serial.println(batteryLevel);
  #endif
  /* Sprawdzenie stanu baterii */
  if(batteryLevel.toInt() < LOW_BATTERY_THRESHOLD && lowBatteryFlag == 0)
  {
    #if DEBUG
      Serial.println("lowBatteryFlag ustawiona");
    #endif
    lowBatteryFlag = 1;
    /* Wysłanie wiadomości tekstowej z komunikatem o niskim poziomie baterii */
    sendSms("Uwaga, niski poziom baterii w urzadzeniu. Podlacz urzadzenie do ladowania.");
  }
  else if (batteryLevel.toInt() > HIGH_BATTERY_THRESHOLD && lowBatteryFlag == 1)
  {
    #if DEBUG
      Serial.println("lowBatteryFlag zresetowana");
    #endif
    /* Reset flagi niskiego poziomu baterii */
    lowBatteryFlag = 0;
  }
}

void A9Gmodule::changeTaskPeriod()
{
  /* Zmiana częstotliwości sprawdzania lokalizacji i wysyłania danych do serwera MQTT */
  uint8_t tempStr = a9gAnswer.indexOf("Period:");
  uint32_t tempPeriod = (a9gAnswer.substring(tempStr + 8, tempStr + 14)).toInt();
  #if DEBUG
    Serial.print("Command ChangeTaskPeriod: ");
    Serial.println(tempPeriod);
  #endif
  if(tempPeriod >= MIN_TIME_PERIOD && tempPeriod <= MAX_TIME_PERIOD)
  {
    executeTaskTimePeriod = tempPeriod;
  }
  else if(tempPeriod < MIN_TIME_PERIOD)
  {
    executeTaskTimePeriod = MIN_TIME_PERIOD;
  }
  else if(tempPeriod > MAX_TIME_PERIOD)
  {
    executeTaskTimePeriod = MAX_TIME_PERIOD;
  }
  #if DEBUG
    Serial.print("ChangeTaskPeriod ustawiona na: ");
    Serial.println(executeTaskTimePeriod);
  #endif
}

void A9Gmodule::prepareAndSaveLocation()
{
  /* Aktualizacja daty i czasu ostatniej lokalizacji */
  lastLocationTime = dateTime;
  /* Przygotowanie i zapis lokalizacji do pamięci */
  separator = a9gAnswer.indexOf(","); //ustawienie zmiennej separującej współrzędne geograficzne
  lastLocation = (a9gAnswer.substring(separator - 9, separator)) + ", " + (a9gAnswer.substring(separator + 1, separator + 10));
  /* Zapis danych do pamięci */
  if(!saveConfiguration(SPIFFS, nrTel, writeAPIKey, channelID, lastLocation, lastLocationTime))
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
  /* Przygotowanie komendy do przesyłu danych MQTT */
  command = "AT+MQTTPUB=\"channels/" + channelID + "/publish/"+ writeAPIKey + "\""+ ","+
  "\"field1=" + (a9gAnswer.substring(separator - 9,separator)) +
    "&field2=" + (a9gAnswer.substring(separator + 1,separator + 10)) + 
    "&field3=" + dateTime +
    "&field4=" + batteryLevel +
    "&status=MQTTPUBLISH" + "\"" + ",0,0,0";
}

A9Gmodule::A9Gmodule()
{
    serial = new SoftwareSerial(A9G_RX, A9G_TX, false);
}

A9Gmodule::~A9Gmodule(){}
