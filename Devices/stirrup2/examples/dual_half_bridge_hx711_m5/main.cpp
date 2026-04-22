#include <M5Unified.h>
#include <HX711.h>

/**
 * Two 3-wire half-bridge load cells + HX711 + M5StickC Plus2
 *
 * Wiring for two half-bridge sensors:
 *   HX711 E+ -> sensor A white + sensor B black
 *   HX711 E- -> sensor A black + sensor B white
 *   HX711 A+ -> sensor A red
 *   HX711 A- -> sensor B red
 *
 * HX711 to M5StickC Plus2:
 *   HX711 DT  -> GPIO33
 *   HX711 SCK -> GPIO32
 *   HX711 VCC -> 5V
 *   HX711 GND -> GND
 *
 * Buttons:
 *   BtnA -> tare (zero with no load)
 *   BtnB -> calibrate with known weight
 */

constexpr int kHx711DataPin = 33;
constexpr int kHx711ClockPin = 32;
constexpr float kCalibrationWeightG = 1000.0f;
constexpr uint8_t kReadSamples = 8;
constexpr uint8_t kTareSamples = 16;
constexpr uint8_t kCalibrationSamples = 20;
constexpr uint32_t kUpdateIntervalMs = 100;

static HX711 s_scale;
static bool s_hxReady;
static bool s_hasWeight;
static bool s_isCalibrated;
static long s_adc;
static long s_offset;
static long s_delta;
static float s_weightG;
static float s_scaleFactor = 1.0f;
static uint32_t s_lastUpdateMs;

static bool tareScale() {
  if (!s_hxReady || !s_scale.is_ready()) {
    return false;
  }

  if (s_isCalibrated) {
    s_scale.set_scale(s_scaleFactor);
  } else {
    s_scale.set_scale();
  }
  s_scale.tare(kTareSamples);
  s_offset = s_scale.get_offset();
  return true;
}

static bool calibrateScale() {
  if (!s_hxReady || !s_scale.is_ready()) {
    return false;
  }

  s_scale.set_scale();
  const long adcAverage = s_scale.read_average(kCalibrationSamples);
  const long delta = adcAverage - s_scale.get_offset();
  if (delta == 0) {
    return false;
  }

  s_scaleFactor = static_cast<float>(delta) / kCalibrationWeightG;
  s_scale.set_scale(s_scaleFactor);
  s_isCalibrated = true;
  return true;
}

static void updateScaleReading() {
  if (!s_hxReady) {
    s_hasWeight = false;
    return;
  }

  if (!s_scale.is_ready()) {
    return;
  }

  s_adc = s_scale.read_average(kReadSamples);
  s_offset = s_scale.get_offset();
  s_delta = s_adc - s_offset;

  if (!s_isCalibrated) {
    s_hasWeight = false;
    s_weightG = 0.0f;
    return;
  }

  s_weightG = static_cast<float>(s_delta) / s_scaleFactor;
  s_hasWeight = true;
}

static void drawUi() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(4, 4);

  M5.Display.setTextColor(TFT_GREEN);
  if (!s_hxReady) {
    M5.Display.println("HX711: offline");
  } else if (!s_isCalibrated) {
    M5.Display.println("WGT : calibrate");
  } else if (s_hasWeight) {
    M5.Display.printf("WGT : % .1f g\n", s_weightG);
  } else {
    M5.Display.println("WGT : hold");
  }

  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.printf("ADC : %ld\n", s_adc);
  M5.Display.printf("OFF : %ld\n", s_offset);
  M5.Display.printf("DEL : %ld\n", s_delta);
  M5.Display.printf("SCL : %.4f\n", static_cast<double>(s_scaleFactor));
  M5.Display.println("A:tare B:cal");
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  Serial.println();
  Serial.printf("HX711 DT=GPIO%d SCK=GPIO%d\n", kHx711DataPin, kHx711ClockPin);
  Serial.printf("Calibration weight = %.1f g\n", kCalibrationWeightG);

  M5.Display.setRotation(1);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE);

  s_scale.begin(kHx711DataPin, kHx711ClockPin);
  delay(500);
  s_hxReady = s_scale.is_ready();

  if (s_hxReady) {
    Serial.println("HX711 ready");
    if (tareScale()) {
      Serial.println("Tare complete, place known weight and press BtnB");
    } else {
      Serial.println("Tare failed");
    }
  } else {
    Serial.println("HX711 not ready");
  }

  drawUi();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    Serial.println("BtnA: tare requested");
    if (tareScale()) {
      Serial.println("Tare complete");
    } else {
      Serial.println("Tare failed");
    }
  }

  if (M5.BtnB.wasPressed()) {
    Serial.printf("BtnB: calibrate with %.1f g\n", kCalibrationWeightG);
    if (calibrateScale()) {
      Serial.printf("Calibration complete, scale=%.6f\n", static_cast<double>(s_scaleFactor));
    } else {
      Serial.println("Calibration failed");
    }
  }

  const uint32_t now = millis();
  if (now - s_lastUpdateMs < kUpdateIntervalMs) {
    return;
  }
  s_lastUpdateMs = now;

  updateScaleReading();
  drawUi();

  Serial.printf("weight=% .1f g adc=%ld offset=%ld delta=%ld scale=%f calibrated=%s\n",
                s_hasWeight ? s_weightG : 0.0f, s_adc, s_offset, s_delta,
                static_cast<double>(s_scaleFactor), s_isCalibrated ? "yes" : "no");
}
