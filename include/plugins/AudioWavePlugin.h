#pragma once

#include "PluginManager.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h" // new ESP-IDF I2S API (IDF >= 5.0 / Arduino core >= 3.x)
#include "dsps_dotprod.h"   // esp-dsp: vectorised dot product
#include "esp_heap_caps.h"
#include <math.h>

// ─── INMP441 I2S pin config ────────────────────────────────────────────────
// Override any of these in constants.h before including this header.
#ifndef AUDIO_WAVE_PIN_SCK
#define AUDIO_WAVE_PIN_SCK GPIO_NUM_5 // BCLK
#endif
#ifndef AUDIO_WAVE_PIN_WS
#define AUDIO_WAVE_PIN_WS GPIO_NUM_4 // LRCLK / WS
#endif
#ifndef AUDIO_WAVE_PIN_SD
#define AUDIO_WAVE_PIN_SD GPIO_NUM_6 // DOUT from mic (DIN to ESP)
#endif

// I2S_NUM_AUTO lets the driver pick a free controller — safest default.
// Override to I2S_NUM_0 or I2S_NUM_1 if you need a specific port.
#ifndef AUDIO_WAVE_I2S_PORT
#define AUDIO_WAVE_I2S_PORT I2S_NUM_AUTO
#endif

// ─── Buffer / LUT sizes ───────────────────────────────────────────────────
// 256 × 4 bytes = 1 KB per buffer (i2sBuf_ and fBuf_).
// Both are allocated in PSRAM on setup() and freed on teardown().
#define AUDIO_WAVE_BUF_SIZE 256

class AudioWavePlugin : public Plugin
{
public:
  AudioWavePlugin();
  ~AudioWavePlugin() override;

  void setup() override;
  void loop() override;
  void teardown() override;
  const char* getName() const override;

  // Websocket hook — runtime tuning via JSON:
  //   { "gain":        <float 0.1–20.0> }  RMS multiplier before normalising
  //   { "noiseFloor":  <float 0.0–0.1>  }  absolute RMS threshold subtracted before scaling
  //   { "gradient":    <float 0.05–1.0> }  brightness falloff gamma; rebuilds LUT on change
  void websocketHook(JsonDocument& request) override;

private:
  // ── I2S (new channel-based API) ───────────────────────────────────────
  i2s_chan_handle_t rxChan_ = nullptr; // null when plugin is inactive
  bool i2sReady_ = false;

  void setupI2S_();
  void teardownI2S_();

  // ── Buffers (PSRAM-preferred, allocated on setup, freed on teardown) ──
  int32_t* i2sBuf_ = nullptr; // raw 32-bit I2S samples from DMA read
  float* fBuf_ = nullptr;     // DC-removed, normalised floats for dsps_dotprod

  float computeRMS_(int samples);

  // ── Signal processing state ───────────────────────────────────────────
  float smoothedRMS_ = 0.0f;
  float peakRMS_ = 0.01f;

  // ── Scrolling history (internal SRAM — 64 bytes, hot every loop()) ────
  float history_[COLS];

  // ── Tunable parameters ────────────────────────────────────────────────
  float gain_ = 0.5f;
  float noiseFloor_ = 0.4f;
  float gradient_ = 2.2f;

  // ── Rendering ─────────────────────────────────────────────────────────
  void renderToScreen_();
  uint8_t pixelBrightness_(int distFromCentre, float normAmp);

  uint8_t frameSkip_ = 4; // ~30fps@4
  uint8_t frameCounter_ = 0;
};
