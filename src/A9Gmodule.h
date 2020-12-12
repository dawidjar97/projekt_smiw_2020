/* Podstawowe biblioteki */
#include <SoftwareSerial.h>
/* Menedżer obsługi pliku konfiguracji */
#include "configurator.h"

/* Mapowanie wyprowadzeń */
#define A9G_PON     16  //ESP12 GPIO16 A9/A9G POWON
#define A9G_POFF    15  //ESP12 GPIO15 A9/A9G POWOFF
#define A9G_WAKE    13  //ESP12 GPIO13 A9/A9G WAKE
#define A9G_LOWP    2  //ESP12 GPIO2 A9/A9G ENTER LOW POWER MODULE
#define A9G_RX      14  //A9G GPS/GPRS UART RX PIN
#define A9G_TX      12  //A9G GPS/GPRS UART TX PIN


/* Predkosc transmisji dla SoftwareSerial */
#define A9_BAUD_RATE 9600

/* Poprawne zakresy czasu dla ExecuteTaskTimePeriod w ms */
#define MIN_TIME_PERIOD 10000   //10 sekund
#define MAX_TIME_PERIOD 900000  //15 minut

/* Progi dla powiadomienia o niskim stanie baterii */
#define LOW_BATTERY_THRESHOLD 20
#define HIGH_BATTERY_THRESHOLD 50

class A9Gmodule
{
private:

    /* Konfiguracja użytkownika */
    String writeAPIKey;
    String channelID;
    String nrTel;

    
    /* Przechowywane dane */
    String lastLocation;
    String lastLocationTime;

    /* Komunikacja */
    SoftwareSerial *serial;

    /* Główne dane */
    bool configFlag = 1;  //flaga konfiguracji urządzenia
    uint8_t reconfigCounter = 0;  //licznik kolejnych rekonfiguracji sieci

    /* Dane */
    String batteryLevel = "";
    bool lowBatteryFlag = 0;
    String dateTime = "";
    String a9gAnswer = "";
    String command = "";
    uint8_t separator=33; //zmienna używana do separacji współrzędnych GPS
    uint32_t executeTaskTimePeriod = 60000; //1minute

    int A9GPOWERON(); //Metoda włączająca moduł GPRS/GPS
    void A9GMQTTCONNECT();  //Metoda inicjująca połączenie do serwera MQTT
    void clearSms();  //Metoda do usuwania wiadomości tekstowych z pamięci modułu GPRS/GPS
    void sendSms(String msg); //Metoda do wysyłania wiadomości tekstowych
    void a9gCommunication(String command, const int timeout); //Metoda do komunikacji dwustronnej z modułem GPRS/GPS
    void checkMessage(); //Metoda do sprawdzenia wiadomości zwrotnej
    void batteryUpdate(); //Metoda do pobierania i sprawdzania stanu baterii
    void changeTaskPeriod(); //Metoda do zmiany okresu wysyłania lokalizacji
    void reset(); //Metoda do resetowania urządzenia do ustawień fabrycznych
    void restart(); //Metoda do restartowania urządzenia
    void prepareAndSaveLocation(); //Metoda do przygotowania lokalizacji do wysłania i zapisu jej w pamięci urządzenia

public:
    A9Gmodule(); //Kontruktor
    ~A9Gmodule();
    
    void setup();   //Metoda do ustawienia modułu w tryb pracy
    void loadData();    //Metoda do wczytywania wartości z pamięci urządzenia
    void executeTask(); //Główna metoda modułu
   
};