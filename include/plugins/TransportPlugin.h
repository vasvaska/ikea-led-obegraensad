#pragma once
#ifdef ESP32
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#endif
#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#endif
#include "PluginManager.h"
#include <ArduinoJson.h>

class TransportPlugin : public Plugin
{
private:
  static const int RBL_ID = 2161;
  static const unsigned long UPDATE_INTERVAL_MS = 35000;
  unsigned long lastUpdate = 0;
  std::vector<int> timetodep;
  bool hasCachedData = false;

  HTTPClient http;
#ifdef ESP32
  WiFiClientSecure *secureClient = nullptr;
  #endif
#ifdef ESP8266
  WiFiClient wiFiClient;
#endif

public:
  ~TransportPlugin()
  {
#ifdef ESP32
    if (secureClient != nullptr)
    {
      delete secureClient;
      secureClient = nullptr;
    }
#endif
  }

  void setup() override;
  void loop() override;
  void teardown() override;
  const char *getName() const override;

private:
  void fetchDepartures();
};