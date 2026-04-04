#pragma once

#include "PluginManager.h"
#include "timing.h"
#include <WiFiClientSecure.h>

class WLClockPlugin : public Plugin
{
private:
  WiFiClientSecure *secureClient = nullptr;
  struct tm timeinfo;
  NonBlockingDelay timer;
  int previousMinutes;
  int previousHour;
  std::vector<int> previousHH;
  std::vector<int> previousMM;
  bool previousLeadingZero;
  std::array<int, 2> departures = {-1, -1};
  void fetchDepartures();
  void drawCooldown();  

public:
  void setup() override;
  void loop() override;
  const char *getName() const override;
};
