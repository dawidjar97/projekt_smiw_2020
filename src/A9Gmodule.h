/* Bacics includes */
#include <Arduino.h>
#include <SoftwareSerial.h>
/* Filesystem manager library */
#include "configurator.h" //bibloteka do obsługi ładowania plików konfiguracyjnych

/* Pin mapping */
#define A9G_PON     16  //ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF    15  //ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE    13  //ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP    2  //ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE
#define A9G_RX      14  //A9G GPS/GPRS UART RX PIN
#define A9G_TX      12  //A9G GPS/GPRS UART TX PIN


/* Software serial data rate speed */
#define A9_BAUD_RATE 9600

/* Correct time range for ExecuteTaskTimePeriod in ms */
#define MIN_TIME_PERIOD 10000   //10 seconds
#define MAX_TIME_PERIOD 900000  //15 minutes

/* Low battery alert thresholds */
#define LOW_BATTERY_THRESHOLD 20
#define HIGH_BATTERY_THRESHOLD 50

class A9Gmodule
{
private:

    /* User config */
    String writeAPIKey;
    String channelID;
    String nrTel;

    uint32_t executedTaskTimePeriod = 240000;   //4minutes
    uint32_t executeTaskTimePeriod = 60000; //1minute
    /* Stored data */
    String lastLocation;
    String lastLocationTime;

    /* Hardware */
    SoftwareSerial *serial;

    /* Head data */
    bool config = 1;  //flaga konfiguracji urządzenia
    uint8_t reconfigCounter = 0;  //licznik kolejnych rekonfiguracji sieci

    /* Data */
    String batteryLevel = "";
    bool lowBatteryFlag = 0;
    String dateTime = "";
    String a9gAnswer = "";
    String command = "";
    uint8_t separator=33; //zmienna używana do separacji współrzędnych GPS

    int A9GPOWERON(); //Funkcja włączająca moduł GPRS/GPS
    void A9GMQTTCONNECT();  //Funkcja inicjująca połączenie do serwera MQTT
    void checkMessage(); //Funkcja do sprawdzenia wiadomości zwrotnej
    void a9gCommunication(String command, const int timeout); //Funkcja do komunikacji dwustronnej z modułem GPRS/GPS
    void clearSms();  //Funkcja do usuwania wiadomości tekstowych z pamięci modułu GPRS/GPS
    void sendSms(String msg); //Funkcja do wysyłania wiadomości tekstowych
    
    void batteryUpdate(); //funkcja do pobierania i sprawdzania stanu baterii
    void changeTaskPeriod(); //funkcja do zmiany okresu wysyłania lokalizacji
    void reset(); //funkcja do resetowania urządzenia do ustawień fabrycznych
    void restart(); //funkcja do restartowania urządzenia

public:
    A9Gmodule(); //Kontruktor
    ~A9Gmodule();
   
    void executeTask(); //Główna funkcja modułu
    void setup();   //Funkcja do ustawienia modułu w tryb pracy
    void loadData();    //funkcja do wczytywania wartości z pamięci urządzenia
   
};