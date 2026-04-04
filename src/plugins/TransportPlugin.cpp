#include "plugins/TransportPlugin.h"

HTTPClient httpWL;

// extern const char* wiener_linien_ca;
void TransportPlugin::setup()
{
#ifdef ESP32
  if (secureClient == nullptr)
  {
    secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
  }
#endif
  // clientWL.setCACert(wiener_linien_ca);
  Screen.clear();

  // Show loading screen - data needs to be fetched
  currentStatus = LOADING;
  Screen.setPixel(4, 7, 1);
  Screen.setPixel(5, 7, 1);
  Screen.setPixel(7, 7, 1);
  Screen.setPixel(8, 7, 1);
  Screen.setPixel(10, 7, 1);
  Screen.setPixel(11, 7, 1);
  fetchDepartures();
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
     httpWL.end();
}

const char *TransportPlugin::getName() const
{
  return "Transport";
}

void TransportPlugin::fetchDepartures()
{
  String payload;
  String wl_endpoint = "https://www.wienerlinien.at/ogd_realtime/monitor?stopId=2161";
  // httpWL.setHandshakeTimeout(20000);
  // httpWL.setTimeout(20000);
  //  Using the dedicated WL client (set to insecure or with CA)
  if (secureClient != nullptr)
  {
    secureClient->setCACert(wiener_linien_ca); // TODOS!
    httpWL.begin(*secureClient, wl_endpoint);
    int httpCode = httpWL.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      Serial.printf("[API] Success, Code: %d\n", httpCode);

      // payload = httpWL.getString();
      Serial.println("Fetching data");

      payload = "";
      int totalLength = httpWL.getSize();
      Serial.printf("Content Length: %d\n", totalLength);
      if (totalLength > 0)
      {
        payload.reserve(totalLength + 1);
      }

      WiFiClient *stream = httpWL.getStreamPtr();
      uint8_t buffer[256]; // Smaller chunks are safer for the PICO/S3 heap

      int bytesRemaining = totalLength;

      Serial.println("[HTTP] Starting streamed read...");
      long startTime = millis();

      while (httpWL.connected() && (bytesRemaining > 0 || totalLength == -1))
      {
        Serial.printf("Time elapsed: %d ms\n", millis() - startTime);
        Serial.printf("Bytes remaining: %d\n", bytesRemaining);
        size_t size = stream->available();
        if (size)
        {
          // Read into buffer
          int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
          payload.concat((char *)buffer, c);
          if (totalLength > 0)
            bytesRemaining -= c;
        }
        else
          break; // No more data available, exit loop

        // This resets the Watchdog Timer and lets other tasks run
        vTaskDelay(1);
      }
      Serial.println("\n[HTTP] Finished streamed read.");
      Serial.printf("total read time: %d ms\n", millis() - startTime);
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

      // Serial.printf("Payload Length: %d\n", payload.length());
    }
    else
    {
      Serial.printf("[API] Error: %s\n", httpWL.errorToString(httpCode).c_str());
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

      Messages.add(std::string(String(httpCode).c_str()) + std::string("Heap: ") +
                   std::to_string(ESP.getMaxAllocHeap()));

      // char error_buf[100];
      // secureClient->lastError(error_buf, 100);
      // Messages.add(std::string("SSL Error: ") + std::string(error_buf));
      // Messages.add(std::string(http.getLocation().c_str()));
      return;
    }
    httpWL.end();
  }
  else
  {
    Serial.println("Error initializing HTTP client");
    return;
  }
  // Serial.println("Finished HTTP request, processing payload...");
  // Serial.print(payload);

  // // --- PARSE ONLY WHAT WE FILTER ---
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::NestingLimit(20));
  if (error)
  {
    Serial.println("\nError deserializing JSON with filter: " + String(error.c_str()));
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
  else
  {
    Screen.clear();
    /* No error */
    auto departures = doc["data"]["monitors"][0]["lines"][0]["departures"]["departure"];

    if (departures.size() > 0)
    {
      int c;
      c = departures[0]["departureTime"]["countdown"];
      // Apply 90-min rule: if >90, force to 0 (which results in 00)
      c = (c > 90 || c < 0) ? 0 : c;

      // Inline draw: {Tens, Ones}
      Screen.drawNumbers(0, 0, {c / 10, c % 10});
      delayMicroseconds(1000);
      if (departures.size() > 1)
      {
        c = departures[1]["departureTime"]["countdown"];
        c = (c > 90 || c < 0) ? 0 : c;

        Screen.drawNumbers(0, 8, {c / 10, c % 10});
        delayMicroseconds(1000);
      }
    }
  }
  // Free up memory used by the payload string and JSON document
  payload = String();
  doc.clear();
}