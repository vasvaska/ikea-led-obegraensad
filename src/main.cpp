#include <Arduino.h>
#include <BfButton.h>
#include <SPI.h>
// extern const char* adafruit_ca;

#ifdef ESP8266
/* Fix duplicate defs of HTTP_GET, HTTP_POST, ... in ESPAsyncWebServer.h */
#define WEBSERVER_H
#endif

#include <WiFiManager.h>

#ifdef ESP32
#include <ESPmDNS.h>
#endif
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

#include "PluginManager.h"
#include "config.h"
#include "scheduler.h"

#include "plugins/Blob.h"
#include "plugins/BubblesPlugin.h"
#include "plugins/CirclePlugin.h"
#include "plugins/CometPlugin.h"
#include "plugins/FireworkPlugin.h"
#include "plugins/LinesPlugin.h"
#include "plugins/RainPlugin.h"
#include "plugins/SparkleFieldPlugin.h"

#include "plugins/MatrixRainPlugin.h"
#include "plugins/MeteorShowerPlugin.h"
#include "plugins/StarsPlugin.h"
#include "plugins/WavePlugin.h"

#ifdef ENABLE_SERVER
#include "plugins/BigClockPlugin.h"
#include "plugins/DrawPlugin.h"
#include "plugins/WLClockPlugin.h"
#include "plugins/WeatherPlugin.h"
#endif

#include "asyncwebserver.h"
#include "messages.h"
#include "ota.h"
#include "screen.h"
#include "secrets.h"
#include "vassecrets.h"
#include "websocket.h"
#include <PubSubClient.h>

// MQTT Stuff
// WiFiClientSecure wifiClientIO;
// PubSubClient mqttClient(wifiClientIO);
// Transport Stuff

volatile uint8_t mqtt_value = 0;

BfButton btn(BfButton::STANDALONE_DIGITAL, PIN_BUTTON, true, LOW);

unsigned long previousMillis = 0;
unsigned long interval = 30000;

PluginManager pluginManager;
#ifdef ESP32
DRAM_ATTR volatile SYSTEM_STATUS currentStatus = NONE;
#else
volatile SYSTEM_STATUS currentStatus = NONE;
#endif
WiFiManager wifiManager;

unsigned long lastConnectionAttempt = 0;
const unsigned long connectionInterval = 10000;
unsigned long reconnectionBackoff = 5000;            // Start with 5 seconds
const unsigned long maxReconnectionBackoff = 300000; // Max 5 minutes
uint8_t reconnectionAttempts = 0;

void connectToWiFi()
{
  // if a WiFi setup AP was started, reboot is required to clear routes
  bool wifiWebServerStarted = false;
  wifiManager.setWebServerCallback([&wifiWebServerStarted]() { wifiWebServerStarted = true; });

  wifiManager.setHostname(WIFI_HOSTNAME);

#if defined(IP_ADDRESS) && defined(GWY) && defined(SUBNET) && defined(DNS1)
  auto ip = IPAddress();
  ip.fromString(IP_ADDRESS);

  auto gwy = IPAddress();
  gwy.fromString(GWY);

  auto subnet = IPAddress();
  subnet.fromString(SUBNET);

  auto dns = IPAddress();
  dns.fromString(DNS1);

  wifiManager.setSTAStaticIPConfig(ip, gwy, subnet, dns);
#endif

  wifiManager.setConnectRetries(10);
  wifiManager.setConnectTimeout(10);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setWiFiAutoReconnect(true);
  wifiManager.autoConnect(WIFI_MANAGER_SSID);

#ifdef ESP32
  if (MDNS.begin(WIFI_HOSTNAME))
  {
    MDNS.addService("http", "tcp", 80);
    MDNS.setInstanceName(WIFI_HOSTNAME);
  }
  else
  {
    Serial.println("Could not start mDNS!");
  }
#endif

  if (wifiWebServerStarted)
  {
    // Reboot required, otherwise wifiManager server interferes with our server
    Serial.println("Done running WiFi Manager webserver - rebooting");
    ESP.restart();
  }

  lastConnectionAttempt = millis();
}

// void mqttCallback(char *topic, byte *payload, unsigned int length)
// {
//   // Payload is NOT null-terminated
//   char value[8] = {0};
//   memcpy(value, payload, min(length, sizeof(value) - 1));

//   // Convert to int (your 1 byte)
//   mqtt_value = atoi(value);
//   if (currentStatus != LOADING)
//   {
//     Scheduler.clearSchedule();
//     pluginManager.setActivePluginById(mqtt_value);
//   }
// }
// void connectMQTT()
// {
//   mqttClient.setServer(MQTT_server, MQTT_port);
//   mqttClient.setCallback(mqttCallback);

//   while (!mqttClient.connected())
//   {
//     if (mqttClient.connect("esp32-client", // client ID (can be anything)
//                            MQTT_username,  // MQTT username
//                            MQTT_key        // MQTT password
//                            ))
//     {

//       // Subscribe once connected
//       mqttClient.subscribe(MQTT_topic);
//     }
//     else
//     {
//       Serial.println("mqttSetupFail");
//     }
//   }
// }

void pressHandler(BfButton *btn, BfButton::press_pattern_t pattern)
{
  Serial.print("Button event: ");
  switch (pattern)
  {
  case BfButton::SINGLE_PRESS:
    if (currentStatus != LOADING)
    {
      Scheduler.clearSchedule();
      pluginManager.activateNextPlugin();
    }
    break;

  case BfButton::LONG_PRESS:
    if (currentStatus != LOADING)
    {
      pluginManager.activatePersistedPlugin();
    }
    break;
  }
}

void baseSetup()
{
  Serial.begin(115200);

  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_DATA, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

#if !defined(ESP32) && !defined(ESP8266)
  Screen.setup();
#endif

  // Initialize configuration system (always safe)
  config.begin();

// server
#ifdef ENABLE_SERVER
  connectToWiFi();

  // set time server using config values
  // configTzTime(config.getTzInfo().c_str(), config.getNtpServer().c_str());
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  initOTA(server);
  initWebsocketServer(server);
  initWebServer();
#endif

  /* Order of plugin IDs, starting from 1 (0 is reserved for no plugin):
  // stars 1
  // lines 2
  // circle 3
  // rain 4
  // firework 5
  // clock 6
  // weather 7
  // custom 8
  // matrix rain 9
  // blob 10
  // wave 11
  // bubbles 12
  // meteor shower 13
  */
  pluginManager.addPlugin(new DrawPlugin());
  pluginManager.addPlugin(new StarsPlugin());
  pluginManager.addPlugin(new LinesPlugin());
  pluginManager.addPlugin(new CirclePlugin());
  pluginManager.addPlugin(new RainPlugin());
  pluginManager.addPlugin(new FireworkPlugin());
#ifdef ENABLE_SERVER
  pluginManager.addPlugin(new BigClockPlugin());

  pluginManager.addPlugin(new WeatherPlugin());
  pluginManager.addPlugin(new WLClockPlugin());

#endif
  pluginManager.addPlugin(new MatrixRainPlugin());
  pluginManager.addPlugin(new BlobPlugin());
  pluginManager.addPlugin(new WavePlugin());
  pluginManager.addPlugin(new BubblesPlugin());
  pluginManager.addPlugin(new MeteorShowerPlugin());

  Screen.clear();
  pluginManager.init();
  Scheduler.init();

  btn.onPress(pressHandler).onDoublePress(pressHandler).onPressFor(pressHandler, 1000);
}

#ifdef ESP32
TaskHandle_t screenDrawingTaskHandle = NULL;

void screenDrawingTask(void *parameter)
{
  Screen.setup();
  for (;;)
  {
    pluginManager.runActivePlugin();
    vTaskDelay(1);
  }
}

void setup()
{
  delay(1000); // Short delay to allow for any serial monitor connection before starting WiFi and mDNS
  baseSetup();
  // wifiClientIO.setCACert(adafruit_ca); // AdafruitIO CA setup for secure MQTT
  // connectMQTT();
  xTaskCreatePinnedToCore(screenDrawingTask,
                          "screenDrawingTask",
                          10000,
                          NULL,
                          1,
                          &screenDrawingTaskHandle,
                          0);
}
#endif
#ifdef ESP8266
void screenDrawingTask()
{
  Screen.setup();
  pluginManager.runActivePlugin();
  yield();
}

void setup()
{
  baseSetup();
  Scheduler.start();
}
#endif

void loop()
{
  static uint8_t taskCounter = 0;

  btn.read();

#ifdef ENABLE_SERVER
  ElegantOTA.loop();
#endif

#if !defined(ESP32) && !defined(ESP8266)
  pluginManager.runActivePlugin();
#endif

  if (currentStatus == NONE)
  {
    Scheduler.update();

    if ((taskCounter & 0x03) == 0)
    {
      Messages.scrollMessageEveryMinute();
    }
  }

  // Check WiFi less frequently with exponential backoff
  if (WiFi.status() != WL_CONNECTED)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - lastConnectionAttempt >= reconnectionBackoff)
    {
      Serial.println("WiFi disconnected, attempting reconnection...");
      connectToWiFi();

      // Exponential backoff: double the wait time, up to max
      reconnectionAttempts++;
      reconnectionBackoff = min(reconnectionBackoff * 2, maxReconnectionBackoff);
    }
  }
  else
  {
    if (reconnectionAttempts > 0)
    {
      Serial.println("WiFi reconnected successfully");
      reconnectionAttempts = 0;
      reconnectionBackoff = 5000;
    }
  }

  // MQTT Loop stuff
  // if (!mqttClient.connected())
  // {
  //   connectMQTT();
  // }

  // mqttClient.loop();
  taskCounter++;
  if (taskCounter > 16)
  {
    taskCounter = 0;
  }

#ifdef ENABLE_SERVER
  cleanUpClients();
#endif
#ifdef ESP32
  vTaskDelay(1);
#else
  delay(1);
#endif
}

/*Helper functions and blocks
//plop free space
Serial.printf("[Memory] Total Heap: %d | Largest Block: %d\n",
              ESP.getFreeHeap(),
              ESP.getMaxAllocHeap());

                  */