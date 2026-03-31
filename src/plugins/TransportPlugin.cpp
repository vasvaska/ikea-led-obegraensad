#include "plugins/TransportPlugin.h"

#include "messages.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

extern WiFiClientSecure wifiClient;
void TransportPlugin::setup()
{
#ifdef ESP32
  // if (secureClient == nullptr)
  // {
  //   secureClient = new WiFiClientSecure();
  //   // secureClient->setCACert(ROOT_CA);
  //   secureClient->setInsecure();
  // }
#endif
  Screen.clear();

  // Show loading screen - data needs to be fetched
  currentStatus = LOADING;
  Screen.setPixel(4, 7, 1);
  Screen.setPixel(5, 7, 1);
  Screen.setPixel(7, 7, 1);
  Screen.setPixel(8, 7, 1);
  Screen.setPixel(10, 7, 1);
  Screen.setPixel(11, 7, 1);
  currentStatus = NONE;
  this->lastUpdate = 0;
}

void TransportPlugin::loop()
{

  if (this->lastUpdate == 0 || millis() >= this->lastUpdate + UPDATE_INTERVAL_MS)
  {
    this->fetchDepartures();
    this->lastUpdate = millis();
  }
}

void TransportPlugin::teardown()
{
}

const char *TransportPlugin::getName() const
{
  return "Transport";
}

void TransportPlugin::fetchDepartures()
{
  http.end();

  //wifiClient.setHandshakeTimeout(20000);
  //wifiClient.setTimeout(20000);
  String url = "https://www.wienerlinien.at/ogd_realtime/monitor?stopId=2161";
  http.setTimeout(20000);
  // Messages.add("+++");
  // if (*wifiClient != nullptr)
  // {
  if (!http.begin(wifiClient, url))
  {
    Serial.println("HTTP begin failed");
    Messages.add("HTTP begin failed");
    http.end();
    return;
  }
  // }
  // else
  // {
  //   Messages.add("Secure client not initialized!");
  //   return;
  // }
  int code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    http.end();

    Serial.println("Error on HTTP request");
    Screen.clear();

    //!
    Screen.setPixel(7, 4, 1);
    Screen.setPixel(8, 4, 1);
    Screen.setPixel(7, 5, 1);
    Screen.setPixel(8, 5, 1);
    Screen.setPixel(7, 6, 1);
    Screen.setPixel(8, 6, 1);
    Screen.setPixel(7, 7, 1);
    Screen.setPixel(8, 7, 1);
    Screen.setPixel(7, 8, 1);
    Screen.setPixel(8, 8, 1);

    Screen.setPixel(7, 10, 1);
    Screen.setPixel(8, 10, 1);
    Screen.setPixel(7, 11, 1);
    Screen.setPixel(8, 11, 1);

    Messages.add(std::string(String(code).c_str()));
    // Messages.add(std::string(http.getLocation().c_str()));
    return;
  }

  // --- JSON FILTER ---
  // JsonDocument filter;
  // filter["data"]["monitors"][0]["lines"][0]["departures"]["departure"][0]["countdown"] = true;
  // filter["data"]["monitors"][0]["lines"][0]["departures"]["departure"][1]["countdown"] = true;

  // // --- PARSE ONLY WHAT WE FILTER ---
  // JsonDocument doc;
  // DeserializationError error =
  //     deserializeJson(doc, http_.getStream(), DeserializationOption::Filter(filter));
  // Messages.add(http_.getString().c_str());
  http.end();
  Messages.add("Fetched data");
  // if (error)
  // {
  //   Messages.add("deserializeJson failed");
  //   Serial.print("Error deserializing JSON: ");
  //   Serial.println(error.c_str());
  //   return;
  // }

  // JsonArray deps = doc["data"]["monitors"][0]["lines"][0]["departures"]["departure"];

  // // Print up to two departures
  // this->timetodep.clear();
  // for (JsonObject dep : deps)
  // {
  //   this->timetodep.push_back(dep["countdown"].as<int>());
  // }
  // if (this->timetodep.size() >= 2)
  // {
  //   Screen.drawBigNumbers(0, 0, std::vector<int>{this->timetodep[0]});
  //   Screen.drawBigNumbers(0, 8, std::vector<int>{this->timetodep[1]});
  // }
  // else if (this->timetodep.size() == 1)
  // {
  //   Screen.drawBigNumbers(0, 0, std::vector<int>{this->timetodep[0]});
  // }
}
