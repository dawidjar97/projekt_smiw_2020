#include <Arduino.h>
#include <SoftwareSerial.h>
#include "configurator.h" //bibloteka do obsługi ładowania plików konfiguracyjnych


#define A9G_PON     16  //ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF    15  //ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE    13  //ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP    2  //ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE
#define INTERNAL_LED 2
#define A9_BAUD_RATE 9600

class A9Gmodule
{
private:
    /* data */
    String writeAPIKey;
    String channelID;
    String nrTel;
    String lastLocation;
    String lastLocationTime;

    SoftwareSerial *serial;
    bool config = 1;  //flaga konfiguracji urządzenia
    uint8_t reconfigCounter = 0;  //licznik kolejnych rekonfiguracji sieci

    String batteryStatus = "";
    String batteryLevel = "";
    String dateTime = "";
    String a9gAnswer = "";
    String command = "";
    uint8_t separator=33; //zmienna używana do separacji współrzędnych GPS
public:
    A9Gmodule();
    ~A9Gmodule();
    int A9GPOWERON(); //Funkcja włączająca moduł GPRS/GPS
    void A9GMQTTCONNECT();  //Funkcja inicjująca połączenie do serwera MQTT
    void checkMessage(); //Funkcja do sprawdzenia wiadomości zwrotnej
    void a9gCommunication(String command, const int timeout); //Funkcja do komunikacji dwustronnej z modułem GPRS/GPS
    void clearSms();  //Funkcja do usuwania wiadomości tekstowych z pamięci modułu GPRS/GPS
    void sendSms(String msg); //Funkcja do wysyłania wiadomości tekstowych
    void executeTask();
    void setup();
    void loadData();
};