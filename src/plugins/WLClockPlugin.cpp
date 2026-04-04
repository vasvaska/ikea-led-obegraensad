#include "plugins/WLClockPlugin.h"
#include "vassecrets.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

void WLClockPlugin::setup()
{
  // loading screen
  Screen.setPixel(4, 7, 1);
  Screen.setPixel(5, 7, 1);
  Screen.setPixel(7, 7, 1);
  Screen.setPixel(8, 7, 1);
  Screen.setPixel(10, 7, 1);
  Screen.setPixel(11, 7, 1);
  delay(10);
  previousMinutes = -1;
  previousHour = -1;
  previousHH.clear();
  previousMM.clear();
  previousLeadingZero = false;
#ifdef ESP32
  if (secureClient == nullptr)
  {
    secureClient = new WiFiClientSecure();
    secureClient->setCACert(wiener_linien_ca);
  }
  fetchDepartures();
#endif
}

void WLClockPlugin::loop()
{
  Serial.print("\nLast time: ");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  if (getLocalTime(&timeinfo))
  {
    // Serial.print(timeinfo.tm_hour);
    // Serial.print(" ");
    // Serial.println(timeinfo.tm_min);
    if (previousHour != timeinfo.tm_hour || previousMinutes != timeinfo.tm_min)
    {
      std::vector<int> hh = {(timeinfo.tm_hour - timeinfo.tm_hour % 10) / 10, timeinfo.tm_hour % 10};
      std::vector<int> mm = {(timeinfo.tm_min - timeinfo.tm_min % 10) / 10, timeinfo.tm_min % 10};
      bool leadingZero = (hh.at(0) == 0);

      bool layoutChanged = (previousHH.empty() || previousLeadingZero != leadingZero);

      if (layoutChanged)
      {

        Screen.clear();
        delay(10);
        if (leadingZero)
        {
          hh.erase(hh.begin());
          Screen.drawBigNumbers(COLS / 2, 0, hh);
          delayMicroseconds(1000);
          Screen.drawBigNumbers(0, ROWS / 2, mm);
        }
        else
        {
          Screen.drawBigNumbers(0, 0, hh);
          delayMicroseconds(1000);
          Screen.drawBigNumbers(0, ROWS / 2, mm);
          delay(10);
        }
      }
      else
      {

        std::vector<int> displayHH = hh;
        if (leadingZero)
        {
          displayHH.erase(displayHH.begin());
          delay(10);
        }

        if (displayHH != previousHH)
        {
          int startX = leadingZero ? COLS / 2 : 0;
          Screen.drawBigNumbers(startX, 0, displayHH);
          delay(10);
        }

        if (mm != previousMM)
        {
          Screen.drawBigNumbers(0, ROWS / 2, mm);
          delay(10);
        }

        previousHH = displayHH;
        previousMM = mm;
      }

      if (layoutChanged)
      {

        std::vector<int> displayHH = hh;
        if (leadingZero)
        {
          displayHH.erase(displayHH.begin());
          delay(10);
        }
        previousHH = displayHH;
        previousMM = mm;
      }

      previousMinutes = timeinfo.tm_min;
      delay(10);
      previousHour = timeinfo.tm_hour;
      delay(10);
      previousLeadingZero = leadingZero;
      delay(10);
      drawCooldown();
    }
  }
  if (timer.isReady(40000))
  {
    fetchDepartures();
    drawCooldown();
  }

  // Screen.drawLine(0, 0, 0, 15, 0); // Clear previous departure indicator1
  // delayMicroseconds(1000);
  // Screen.drawLine(0, 15, 0, min(15, departures[0]), 1); // Draw new departure indicator Line1
  // delayMicroseconds(1000);
  // Screen.drawLine(0, 15, 15, 15, 0); // clear previous departure indicator2
  // delayMicroseconds(1000);
  // Screen.drawLine(0, 15, min(15, departures[1]), 15, 1); // Draw new departure indicator Line2
}

const char* WLClockPlugin::getName() const
{
  return "WL Clock";
}
void WLClockPlugin::fetchDepartures()
{
  // Reset departures to invalid values
  // departures[0] = -1;
  // departures[1] = -1;

  // Initialize HTTP client and SSL context
  HTTPClient http;
  // WiFiClientSecure* secureClient = new WiFiClientSecure();
  Serial.println("Fetching departures from Wiener Linien API...");
  if (secureClient == nullptr)
  {
    Serial.println("Error: Failed to allocate WiFiClientSecure");
    return;
  }
  secureClient->setCACert(wiener_linien_ca);
  // secureClient->setCACert(wiener_linien_ca);
  // secureClient->setInsecure(); // For simplicity; replace with CA cert in production

  // Prepare endpoint
  // String endpoint = WIENER_LINIEN_URL;

  // Start HTTP request
  http.begin(*secureClient, WIENER_LINIEN_URL);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("[API] Error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return;
  }

  // Stream response to avoid memory issues
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[256];
  String payload;
  payload.reserve(1024); // Reserve initial space

  int bytesRemaining = http.getSize();
  while (http.connected() && (bytesRemaining > 0 || http.getSize() == -1))
  {
    size_t size = stream->available();
    if (size)
    {
      int c = stream->readBytes(buffer, (size > sizeof(buffer)) ? sizeof(buffer) : size);
      payload.concat((char*)buffer, c);
      if (http.getSize() > 0)
        bytesRemaining -= c;
    }
    vTaskDelay(1); // Yield to other tasks
  }

  // Clean up HTTP client
  http.end();

  // Parse JSON to extract departures
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::NestingLimit(20));
  if (error)
  {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    return;
  }

  // Extract next two departures
  auto jsonDepartures = doc["data"]["monitors"][0]["lines"][0]["departures"]["departure"];
  for (int i = 0; i < 2 && i < jsonDepartures.size(); i++)
  {
    int countdown = jsonDepartures[i]["departureTime"]["countdown"];
    departures[i] = (countdown > 90 || countdown < 0) ? 0 : countdown;
  }

  // Free memory
  doc.clear();
  payload = String();
}
void WLClockPlugin::drawCooldown()
{
  Screen.drawLine(0, 0, 0, 15, 0, 0); // Clear previous departure indicator1
  delay(1);
  Screen.drawLine(0, 15, 15, 15, 0, 0); // clear previous departure indicator2
  delay(1);

  Screen.drawLine(0, 15, 0, 15 - min(15, departures[0]), 1, 100); // Draw new departure indicator Line1
  delay(1);
  Screen.drawLine(0, 15, min(15, departures[1]), 15, 1, 100); // Draw new departure indicator Line2
}