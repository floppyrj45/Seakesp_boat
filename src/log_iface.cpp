#include "log_iface.h"
#include "runtime_config.h"

// Déclaration externe du niveau de log global depuis main.cpp
enum LogLevel { LOG_ERROR=0, LOG_WARN=1, LOG_LOW=2, LOG_INFO=3, LOG_DEBUG=4 };
extern volatile LogLevel gLogLevel;

void setLogLevelByName(const String& name, bool persist) {
  LogLevel newLevel = LOG_LOW; // défaut
  
  if (name.equalsIgnoreCase("ERROR")) {
    newLevel = LOG_ERROR;
  } else if (name.equalsIgnoreCase("WARN")) {
    newLevel = LOG_WARN;
  } else if (name.equalsIgnoreCase("LOW")) {
    newLevel = LOG_LOW;
  } else if (name.equalsIgnoreCase("INFO")) {
    newLevel = LOG_INFO;
  } else if (name.equalsIgnoreCase("DEBUG")) {
    newLevel = LOG_DEBUG;
  }
  
  gLogLevel = newLevel;
  
  if (persist) {
    saveLogLevelToPrefs((uint8_t)newLevel);
  }
}

String getLogLevelName() {
  switch (gLogLevel) {
    case LOG_ERROR: return "ERROR";
    case LOG_WARN: return "WARN";
    case LOG_LOW: return "LOW";
    case LOG_INFO: return "INFO";
    case LOG_DEBUG: return "DEBUG";
    default: return "LOW";
  }
}



