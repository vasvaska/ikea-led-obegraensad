#include "plugins/WeatherPlugin.h"
#include "config.h"
#include "vassecrets.h"

// https://github.com/chubin/wttr.in/blob/master/share/translations/en.txt
#ifdef ESP32
#include <WiFi.h>
#endif
#ifdef ESP8266
#include <ESP8266WiFi.h>
WiFiClient wiFiClient;
#endif
WiFiClientSecure secureClient_wittr;
void WeatherPlugin::setup()
{
  Screen.clear();

#ifdef ESP32
  //.if (secureClient_wittr == nullptr)
  {
    secureClient_wittr = WiFiClientSecure();
    // secureClient_wittr.setInsecure();
    secureClient_wittr.setCACert(wttr_ca);
  }
#endif

  // If we have cached data and it's still fresh (< 30 minutes old), redraw it
  if (hasCachedData && lastUpdate > 0 && millis() >= lastUpdate &&
      millis() - lastUpdate < (1000UL * 60 * 30))
  {
    Serial.println("Using cached weather data");
    Serial.printf("\nCached Weather Data - Temp: %d°C, Code: %d\n", cachedTemperature, cachedWeatherIcon);
    drawWeather();
  }
  else
  {
    // Show loading screen - data needs to be fetched
    currentStatus = LOADING;
    Screen.setPixel(4, 7, 1);
    Screen.setPixel(5, 7, 1);
    Screen.setPixel(7, 7, 1);
    Screen.setPixel(8, 7, 1);
    Screen.setPixel(10, 7, 1);
    Screen.setPixel(11, 7, 1);
    currentStatus = NONE;

    // Clear lastUpdate to force immediate fetch on first loop
    this->lastUpdate = 0;
  }
}

void WeatherPlugin::loop()
{
  if (this->lastUpdate == 0 || millis() >= this->lastUpdate + (1000 * 60 * 30))
  {
    Serial.println("updating weather");
    this->update();
    this->lastUpdate = millis();
  };
}

void WeatherPlugin::update()
{
  // Check WiFi connection first
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected, skipping weather update");
    return;
  }

  String weatherLocation = config.getWeatherLocation();
  Serial.print("[WeatherPlugin] Fetching weather for configured city: ");
  Serial.println(weatherLocation);

  String weatherApiString = "https://wttr.in/" + weatherLocation + "?format=j2&lang=en";
  Serial.print("[WeatherPlugin] API request: ");
  Serial.println(weatherApiString);

#ifdef ESP32
  //.if (secureClient_wittr == nullptr)
  {
    http.begin(secureClient_wittr, weatherApiString);
  }
  // else
  // {
  //   Serial.println("Secure client not initialized!");
  //   return;
  // }
#endif
#ifdef ESP8266
  http.begin(wiFiClient, weatherApiString);
#endif

  http.setTimeout(20000);

  Serial.println("Sending HTTP GET request...");
  int code = http.GET();
  Serial.print("HTTP response code: ");
  Serial.println(code);

  if (code == HTTP_CODE_OK)
  {
    String payload = "";
    int totalLength = http.getSize();

    if (totalLength > 0)
    {
      payload.reserve(totalLength + 1);
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[128]; // Smaller chunks are safer for the PICO/S3 heap

    int bytesRemaining = totalLength;

    Serial.println("[HTTP] Starting streamed read...");

    while (http.connected() && (bytesRemaining > 0 || totalLength == -1))
    {
      size_t size = stream->available();
      if (size)
      {
        // Read into buffer
        int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
        payload.concat((char *)buffer, c);
        if (totalLength > 0)
          bytesRemaining -= c;
      }
      // This resets the Watchdog Timer and lets other tasks run
      vTaskDelay(1);
    }
    // 1. Find the first '{' and cut off the hex headers
    int firstBrace = payload.indexOf('{');
    int lastBrace = payload.lastIndexOf('}');

    if (firstBrace != -1 && lastBrace != -1)
    {
      payload = payload.substring(firstBrace, lastBrace + 1);
    }
    payload.trim();
    // Check for "ghost" null terminators that stop the JSON parser
    for (int i = 0; i < payload.length(); i++)
    {
      if (payload[i] == '\0')
      {
        payload[i] = ' ';
      }
    }

    /* Deserialisation starts here */
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {

      Serial.printf("Payload Length: %d\n", payload.length());
      if (payload.length() > 0)
      {
        // Print the first 50 characters as a string
        Serial.print("Start of payload: ");
        Serial.println(payload.substring(0, min((int)payload.length(), 50)));

        // Print the first 10 bytes in HEX
        Serial.print("First 10 bytes (HEX): ");
        for (int i = 0; i < min((int)payload.length(), 10); i++)
        {
          Serial.printf("%02X ", (unsigned char)payload[i]);
        }
        Serial.println();
      }
      else
      {
        Serial.println("WARNING: Payload is EMPTY!");
      }
      return;
    }

    /* Error free at this point, hopefully. */
    int temperature = round(doc["data"]["current_condition"][0]["temp_C"].as<float>());
    int weatherCode = doc["data"]["current_condition"][0]["weatherCode"].as<int>();
    int weatherIcon = 0;
    int iconY = 1;
    int tempY = 10;
    Serial.printf("\nParsed Weather Data - Temp: %d°C, Code: %d\n", temperature, weatherCode);
    // Free up memory used by the payload string and JSON document
    payload = String();
    doc.clear();

    if (std::find(thunderCodes.begin(), thunderCodes.end(), weatherCode) != thunderCodes.end())
    {
      weatherIcon = 1;
    }
    else if (std::find(rainCodes.begin(), rainCodes.end(), weatherCode) != rainCodes.end())
    {
      weatherIcon = 4;
    }
    else if (std::find(snowCodes.begin(), snowCodes.end(), weatherCode) != snowCodes.end())
    {
      weatherIcon = 5;
    }
    else if (std::find(fogCodes.begin(), fogCodes.end(), weatherCode) != fogCodes.end())
    {
      weatherIcon = 6;
      iconY = 2;
    }
    else if (std::find(clearCodes.begin(), clearCodes.end(), weatherCode) != clearCodes.end())
    {
      weatherIcon = 2;
      iconY = 1;
      tempY = 9;
    }
    else if (std::find(cloudyCodes.begin(), cloudyCodes.end(), weatherCode) != cloudyCodes.end())
    {
      weatherIcon = 0;
      iconY = 2;
      tempY = 9;
    }
    else if (std::find(partyCloudyCodes.begin(), partyCloudyCodes.end(), weatherCode) !=
             partyCloudyCodes.end())
    {
      weatherIcon = 3;
      iconY = 2;
    }

    // Cache the weather data
    hasCachedData = true;
    cachedTemperature = temperature;
    cachedWeatherIcon = weatherIcon;
    cachedIconY = iconY;
    cachedTempY = tempY;

    // Draw the weather
    drawWeather();
  }
  else
  {
    Serial.print("HTTP request failed with code: ");
    Serial.println(code);

    Screen.clear();

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
  }

  http.end();
}

void WeatherPlugin::teardown()
{
  http.end();
}

void WeatherPlugin::drawWeather()
{
  Screen.clear();
  Screen.drawWeather(0, cachedIconY, cachedWeatherIcon, 100);

  int temperature = cachedTemperature;
  int tempY = cachedTempY;

  if (temperature >= 10)
  {
    Screen.drawCharacter(9, tempY, Screen.readBytes(degreeSymbol), 4, 50);
    Screen.drawNumbers(1, tempY, {(temperature - temperature % 10) / 10, temperature % 10});
  }
  else if (temperature <= -10)
  {
    Screen.drawCharacter(0, tempY, Screen.readBytes(minusSymbol), 4);
    Screen.drawCharacter(11, tempY, Screen.readBytes(degreeSymbol), 4, 50);
    temperature *= -1;
    Screen.drawNumbers(3, tempY, {(temperature - temperature % 10) / 10, temperature % 10});
  }
  else if (temperature >= 0)
  {
    Screen.drawCharacter(7, tempY, Screen.readBytes(degreeSymbol), 4, 50);
    Screen.drawNumbers(4, tempY, {temperature});
  }
  else
  {
    Screen.drawCharacter(0, tempY, Screen.readBytes(minusSymbol), 4);
    Screen.drawCharacter(9, tempY, Screen.readBytes(degreeSymbol), 4, 50);
    Screen.drawNumbers(3, tempY, {-temperature});
  }
}

const char *WeatherPlugin::getName() const
{
  return "Weather";
}
