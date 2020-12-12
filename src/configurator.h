#pragma once

#include <Arduino.h>
#include <FS.h>
#include <stdio.h>
#define DEBUG 1 //flaga debugowania
#define INTERNAL_LED 2  //wyprowadzenia do zintegrowanej diody LED
#define CONFIG_PATH "/cfg.file" //Plik do przechowywania konfiguracji

void deleteFile(fs::FS &fs, const char *path);  //funkcja do usuwania plików
bool isConfigurationCompleted(fs::FS &fs);  //funkcja do sprawdzania czy plik istnieje
bool saveConfiguration(fs::FS &fs, const String &nrTel, const String &writeAPIKey, const String &channelID, const String &lastLocation, const String &lastLocationTime);    //funkcja do zapisu konfiguracji
bool loadConfiguration(fs::FS &fs, String &nrTel, String &writeAPIKey, String &channelID, String &lastLocation, String &lastLocationTime);  //funkcja do odczytu konfiguracji