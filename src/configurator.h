#include <Arduino.h>
#include "FS.h"

#define DEBUG 1
#define CONFIG_PATH "/cfg.file"

void deleteFile(fs::FS &fs, const char *path);
bool isConfigurationCompleted(fs::FS &fs);
bool saveConfiguration(fs::FS &fs, const String & nrTel, const String & writeAPIKey, const String & channelID, const String & location);
bool loadConfiguration(fs::FS &fs, String & nrTel, String & writeAPIKey, String & channelID, String & location);