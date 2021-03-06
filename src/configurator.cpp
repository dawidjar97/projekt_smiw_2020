#include "configurator.h"

void deleteFile(fs::FS &fs, const char *path = CONFIG_PATH) //funkcja do usuwania plików
{
    #if DEBUG
        Serial.printf("Deleting file: %s\r\n", path);
    #endif

    if (fs.remove(path))
    {
        #if DEBUG
            Serial.println("Cfg file deleted");
        #endif
    }
    else
    {
        #if DEBUG
            Serial.println("Cfg file delete failed");
        #endif
    }
}

bool isConfigurationCompleted(fs::FS &fs)   //funkcja do sprawdzania czy plik istnieje
{
    return fs.exists(CONFIG_PATH);
}

bool saveConfiguration(fs::FS &fs, const String &nrTel, const String &writeAPIKey, const String &channelID, const String &lastLocation, const String &lastLocationTime) //funkcja do zapisu konfiguracji
{ 
    File file = fs.open(CONFIG_PATH, "w");
    if(!file || file.isDirectory()) 
    {
        #if DEBUG
            Serial.println("Cannot open config file or is directory");
        #endif
        return false;
    }

    file.print(nrTel); file.print('\n');
    file.print(writeAPIKey); file.print('\n');
    file.print(channelID); file.print('\n');
    file.print(lastLocation); file.print('\n');
    file.print(lastLocationTime); file.print('\n');
    file.close();
    return true;
}

bool loadConfiguration(fs::FS &fs, String &nrTel, String &writeAPIKey, String &channelID, String &lastLocation, String &lastLocationTime)   //funkcja do odczytu konfiguracji
{
    if(!fs.exists(CONFIG_PATH))
    {
        #if DEBUG
            Serial.println("Config file doesn't exist");
        #endif
        return false;
    }

    File file = fs.open(CONFIG_PATH, "r");
    if(!file || file.isDirectory()) 
    {
        #if DEBUG
            Serial.println("Cannot open config file or is directory");
        #endif
        return false;
    }
    
    nrTel = file.readStringUntil('\n');
    writeAPIKey = file.readStringUntil('\n');
    channelID = file.readStringUntil('\n');
    lastLocation = file.readStringUntil('\n');
    lastLocationTime = file.readStringUntil('\n');

    #if DEBUG
        Serial.print("nrTel: "); Serial.println(nrTel);
        Serial.print("writeAPIKey: "); Serial.println(writeAPIKey);
        Serial.print("channelID: "); Serial.println(channelID);
        Serial.print("last location: "); Serial.println(lastLocation);
        Serial.print("last location time: "); Serial.println(lastLocationTime);
    #endif

    return true;
}