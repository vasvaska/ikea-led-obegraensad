#include "plugins/AudioWavePlugin.h"
#include "constants.h"

AudioWavePlugin::AudioWavePlugin()
{
  memset(history_, 0, sizeof(history_));
}

AudioWavePlugin::~AudioWavePlugin()
{
  teardownI2S_();
}

const char* AudioWavePlugin::getName() const
{
  return "Audio Wave";
}

void AudioWavePlugin::setup()
{
  memset(history_, 0, sizeof(history_));
  smoothedRMS_ = 0.0f;
  peakRMS_ = 0.01f;
  setupI2S_();
}

void AudioWavePlugin::teardown()
{
  teardownI2S_();
}

void AudioWavePlugin::loop()
{
  if (!i2sReady_ || i2sBuf_ == nullptr || fBuf_ == nullptr)
    return;

  size_t bytesRead = 0;
  esp_err_t err =
      i2s_channel_read(rxChan_,
                       (void*)i2sBuf_,
                       AUDIO_WAVE_BUF_SIZE * sizeof(int32_t),
                       &bytesRead,
                       pdMS_TO_TICKS(20) // 20 ms timeout — non-blocking enough for the plugin loop
      );

  if (err != ESP_OK || bytesRead < sizeof(int32_t))
    return;

  int samplesRead = (int)(bytesRead / sizeof(int32_t));

  // 1. RMS with gain applied (vectorised on S3 via esp-dsp)
  float rms = computeRMS_(samplesRead) * gain_;

  // 2. Subtract noise floor
  rms -= noiseFloor_;
  if (rms < 0.0f)
    rms = 0.0f;

  // 3. Smooth: fast attack, slow decay
  if (rms > smoothedRMS_)
    smoothedRMS_ = rms * 0.6f + smoothedRMS_ * 0.4f;
  else
    smoothedRMS_ = rms * 0.08f + smoothedRMS_ * 0.92f;

  // 4. Auto-gain peak tracker.
  // Decay rate is coupled to gain_: low gain = fast decay (AGC does the work),
  // high gain = slow decay (signal stays raw longer before being pulled back).
  // Range: gain_ 0.1 → decay ~0.980 (fast), gain_ 1.0 → decay ~0.997 (current default)
  float peakDecay = 0.980f + (gain_ - 0.1f) / (1.0f - 0.1f) * (0.997f - 0.980f);
  peakDecay = fmaxf(0.980f, fminf(0.997f, peakDecay));

  if (smoothedRMS_ > peakRMS_)
    peakRMS_ = smoothedRMS_;
  peakRMS_ *= peakDecay;
  if (peakRMS_ < 0.001f)
    peakRMS_ = 0.001f;

  // 5. Normalise 0–1
  float normalised = smoothedRMS_ / peakRMS_;
  if (normalised > 1.0f)
    normalised = 1.0f;

  // 6. Scroll history left, push new amplitude column on the right
  if (++frameCounter_ >= frameSkip_)
  {
    frameCounter_ = 0;
    memmove(&history_[0], &history_[1], (COLS - 1) * sizeof(float));
    history_[COLS - 1] = normalised;
    // 7. Render
    renderToScreen_();
  }
}

// ─── Websocket hook ────────────────────────────────────────────────────────

void AudioWavePlugin::websocketHook(JsonDocument& request)
{
  if (!request["gain"].isNull())
  {
    float v = request["gain"].as<float>();
    if (v >= 0.01f && v <= 2.0f)
      gain_ = v;
  }
  if (!request["noiseFloor"].isNull())
  {
    float v = request["noiseFloor"].as<float>();
    if (v >= 0.0f && v <= 0.5f)
      noiseFloor_ = v;
  }
  if (!request["gradient"].isNull())
  {
    float v = request["gradient"].as<float>();
    if (v >= 0.0f && v <= 3.0f)
    {
      gradient_ = v;
    }
  }
}

// ─── I2S setup / teardown (new channel-based API) ─────────────────────────
//
// New API lifecycle (IDF >= 5.0, Arduino ESP32 core >= 3.x):
//
//   i2s_new_channel()            allocate & register the RX channel handle
//   i2s_channel_init_std_mode()  configure clocks, slots, GPIO
//   i2s_channel_enable()         start DMA, transition to RUNNING state
//   i2s_channel_read()           blocking read into user buffer
//   i2s_channel_disable()        stop DMA, transition back to READY
//   i2s_del_channel()            release handle and all driver resources
//
// INMP441 specifics:
//   - Left-justified 24-bit data in a 32-bit slot (MSB format)
//   - L/R pin tied to GND → data appears on the LEFT channel
//   - slot_mode MONO + slot_mask LEFT reads only the relevant channel,
//     avoiding stereo interleave we'd have to de-interleave manually
//   - No MCLK needed (I2S_GPIO_UNUSED)

void AudioWavePlugin::setupI2S_()
{
  if (i2sReady_)
    return;

  // ── Allocate buffers ──────────────────────────────────────────────────
  const size_t bufBytes = AUDIO_WAVE_BUF_SIZE * sizeof(int32_t);

  // Prefer PSRAM (safe here — i2s_channel_read() is a CPU copy, not DMA-to-pointer)
  if (psramFound())
  {
    i2sBuf_ = (int32_t*)heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    fBuf_ = (float*)heap_caps_malloc(bufBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (i2sBuf_ == nullptr)
    i2sBuf_ = (int32_t*)malloc(bufBytes);
  if (fBuf_ == nullptr)
    fBuf_ = (float*)malloc(bufBytes);

  if (i2sBuf_ == nullptr || fBuf_ == nullptr)
  {
    // Allocation failed — free whatever succeeded and bail
    if (i2sBuf_)
    {
      heap_caps_free(i2sBuf_);
      i2sBuf_ = nullptr;
    }
    if (fBuf_)
    {
      heap_caps_free(fBuf_);
      fBuf_ = nullptr;
    }
    return;
  }

  memset(i2sBuf_, 0, bufBytes);

  // ── Step 1: allocate the RX channel ──────────────────────────────────
  // Pass NULL for tx_handle — RX-only simplex mode.
  // I2S_NUM_AUTO lets the driver pick a free controller automatically.
  i2s_chan_config_t chanCfg =
      I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)AUDIO_WAVE_I2S_PORT, I2S_ROLE_MASTER);
  chanCfg.dma_desc_num = 4;    // number of DMA descriptors (was dma_buf_count)
  chanCfg.dma_frame_num = 128; // frames per DMA buffer (was dma_buf_len)

  if (i2s_new_channel(&chanCfg, NULL, &rxChan_) != ESP_OK)
  {
    heap_caps_free(i2sBuf_);
    i2sBuf_ = nullptr;
    heap_caps_free(fBuf_);
    fBuf_ = nullptr;
    return;
  }

  // ── Step 2: configure standard mode for INMP441 ───────────────────────
  i2s_std_config_t stdCfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
      .slot_cfg =
          {
              // INMP441 outputs 24-bit data left-justified in a 32-bit slot (MSB format).
              // bit_shift = false → MSB format (no 1-bit WS phase shift).
              // left_align = true → data is left-aligned (MSB) within the 32-bit slot.
              .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
              .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, // matches data_bit_width = 32
              .slot_mode = I2S_SLOT_MODE_MONO,
              .slot_mask = I2S_STD_SLOT_LEFT, // L/R pin → GND = left channel
              .ws_width = 32,
              .ws_pol = false,
              .bit_shift = false, // MSB (not Philips) format
              .left_align = true,
              .big_endian = false,
              .bit_order_lsb = false,
          },
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED, // INMP441 doesn't need MCLK
              .bclk = (gpio_num_t)AUDIO_WAVE_PIN_SCK,
              .ws = (gpio_num_t)AUDIO_WAVE_PIN_WS,
              .dout = I2S_GPIO_UNUSED, // TX not used
              .din = (gpio_num_t)AUDIO_WAVE_PIN_SD,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };

  if (i2s_channel_init_std_mode(rxChan_, &stdCfg) != ESP_OK)
  {
    i2s_del_channel(rxChan_);
    rxChan_ = nullptr;
    heap_caps_free(i2sBuf_);
    i2sBuf_ = nullptr;
    heap_caps_free(fBuf_);
    fBuf_ = nullptr;
    return;
  }

  // ── Step 3: enable (start DMA) ────────────────────────────────────────
  if (i2s_channel_enable(rxChan_) != ESP_OK)
  {
    i2s_del_channel(rxChan_);
    rxChan_ = nullptr;
    heap_caps_free(i2sBuf_);
    i2sBuf_ = nullptr;
    heap_caps_free(fBuf_);
    fBuf_ = nullptr;
    return;
  }

  i2sReady_ = true;
}

void AudioWavePlugin::teardownI2S_()
{
  if (i2sReady_)
  {
    i2s_channel_disable(rxChan_); // stop DMA, back to READY state
    i2sReady_ = false;
  }
  if (rxChan_ != nullptr)
  {
    i2s_del_channel(rxChan_); // release driver resources
    rxChan_ = nullptr;
  }
  // heap_caps_free() is safe on both PSRAM and internal heap pointers
  if (i2sBuf_ != nullptr)
  {
    heap_caps_free(i2sBuf_);
    i2sBuf_ = nullptr;
  }
  if (fBuf_ != nullptr)
  {
    heap_caps_free(fBuf_);
    fBuf_ = nullptr;
  }
}

// ─── Signal processing ────────────────────────────────────────────────────
//
// RMS via esp-dsp vectorised dot product:
//
//   1. Scalar loop: compute integer mean (DC removal), then convert
//      int32 → float and normalise to -1..1 in fBuf_.
//      (No esp-dsp equivalent handles int32 mean + conversion in one pass.)
//
//   2. dsps_dotprod_f32(fBuf_, fBuf_, &sumSq, N)
//      Computes Σ(x²) using the S3 PIE SIMD unit (~432 cycles for N=256
//      vs ~1311 cycles scalar — Espressif benchmark, -O2).
//
//   3. sqrtf(sumSq / N) — single scalar call.

float AudioWavePlugin::computeRMS_(int samples)
{
  // Step 1: DC removal + int32 → float normalisation
  int64_t sum = 0;
  for (int i = 0; i < samples; i++)
    sum += (int32_t)i2sBuf_[i];

  float mean = (float)sum / (float)samples;
  float scale = 1.0f / 8388608.0f; // 1 / 2^23 — INMP441 is left-justified 24-bit

  for (int i = 0; i < samples; i++)
    fBuf_[i] = ((float)(int32_t)i2sBuf_[i] - mean) * scale;

  // Step 2: vectorised sum-of-squares
  float sumSq = 0.0f;
  dsps_dotprod_f32(fBuf_, fBuf_, &sumSq, samples);

  // Step 3: finalise RMS
  return sqrtf(sumSq / (float)samples);
}

// ─── Rendering ────────────────────────────────────────────────────────────

uint8_t AudioWavePlugin::pixelBrightness_(int dist, float normAmp)
{
  // S = number of active pixels on each side (0–8)
  int S = (int)(normAmp * (float)(ROWS / 2));
  if (S == 0 || dist >= S)
    return 0;

  // Single active pixel — full brightness
  if (S == 1)
    return 255;

  // gradient_ = 0 → pure on/off
  if (gradient_ < 0.01f)
    return 255;

  // Normalised position: 0.0 (brightest) → 1.0 (dimmest)
  float t = (float)dist / (float)(S - 1);

  // Gamma-corrected brightness: B = 255 * (1 - t)^gamma
  return (uint8_t)((MAX_BRIGHTNESS)*powf(1.0f - t, gradient_));
}

void AudioWavePlugin::renderToScreen_()
{
  for (int col = 0; col < COLS; col++)
  {
    float amp = history_[col];
    for (int dist = 0; dist < ROWS / 2; dist++)
    {
      uint8_t bri = pixelBrightness_(dist, amp);
      Screen.setPixel(col, (ROWS / 2 - 1) - dist, 1, bri);
      Screen.setPixel(col, (ROWS / 2) + dist, 1, bri);
    }
  }
}
