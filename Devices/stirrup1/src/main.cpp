/*
 * M5StickC Plus2 + HX711 scale reader
 * - DAT/DOUT -> GPIO0
 * - CLK/SCK  -> GPIO26
 * - LiDAR RX -> GPIO32
 * - LiDAR TX -> GPIO33
 * - BtnA     -> tare
 */

#include <M5Unified.h>
#include <Preferences.h>
#include "HX711.h"

constexpr int kLoadCellDoutPin = 0; //数据线
constexpr int kLoadCellSckPin = 26; //时钟线
constexpr float kDefaultScale = 1.8f; //重量系数，需要测量，基本可用
constexpr uint32_t kReadyTimeoutMs = 200;
constexpr uint32_t kHoldToCaptureMs = 5000;
constexpr uint32_t kCaptureDurationMs = 5000;
constexpr int kLidarRxPin = 33;
constexpr int kLidarTxPin = 32;
constexpr uint32_t kLidarBaud = 460800;
constexpr uint32_t kOutputIntervalMs = 20;      // 50 Hz combined output
constexpr uint32_t kImuIntervalMs = 20;         // 50 Hz IMU updates
constexpr uint32_t kHx711PollIntervalMs = 5;    // lightweight polling
constexpr float kWeightEmaAlpha = 0.25f;        // smooth single-sample HX711 updates
constexpr float kDisplayedZeroDeadbandKg = 0.020f;

static HX711 scale;
static Preferences preferences;
static M5Canvas canvas(&M5.Display);
static bool s_scaleReady = false;
static uint8_t s_lidarFrame[4];
static uint8_t s_lidarFrameIdx = 0;
static bool s_hasDistance = false;
static bool s_outOfRange = false;
static uint16_t s_lastDistanceMm = 0;
static bool s_btnAHoldHandled = false;
static bool s_btnBHoldHandled = false;
static bool s_hasZeroReference = false;
static bool s_hasImu = false;
static float s_accelX = 0.0f;
static float s_accelY = 0.0f;
static float s_accelZ = 0.0f;
static float s_gyroX = 0.0f;
static float s_gyroY = 0.0f;
static float s_gyroZ = 0.0f;
static float s_savedScale = kDefaultScale;
static long s_savedOffset = 0;
static long s_savedZeroBandMin = 0;
static long s_savedZeroBandMax = 0;
static bool s_hasWeightSample = false;
static long s_lastWeightRaw = 0;
static long s_lastWeightOffset = 0;
static long s_lastWeightDelta = 0;
static float s_lastWeightKg = 0.0f;
static float s_displayWeightKg = 0.0f;
static uint32_t s_lastImuUpdateMs = 0;
static uint32_t s_lastHx711PollMs = 0;
static uint32_t s_lastOutputMs = 0;

static void updateLidar();

static float applyDisplayedZeroDeadband(float weightKg) {
    if (fabsf(weightKg) < kDisplayedZeroDeadbandKg) {
        return 0.0f;
    }
    return weightKg;
}

static void saveCalibration() {
    preferences.putFloat("scale", s_savedScale);
    preferences.putLong("offset", s_savedOffset);
    preferences.putLong("zero_min", s_savedZeroBandMin);
    preferences.putLong("zero_max", s_savedZeroBandMax);
    preferences.putBool("has_zero", s_hasZeroReference);
}

static void loadCalibration() {
    s_savedScale = preferences.getFloat("scale", kDefaultScale);
    s_savedOffset = preferences.getLong("offset", 0);
    s_savedZeroBandMin = preferences.getLong("zero_min", s_savedOffset);
    s_savedZeroBandMax = preferences.getLong("zero_max", s_savedOffset);
    s_hasZeroReference = preferences.getBool("has_zero", false);
}

static void drawCenteredText(const String& text, int32_t y, uint8_t textSize, uint16_t color) {
    canvas.setTextSize(textSize);
    canvas.setTextColor(color);
    canvas.drawString(text, canvas.width() / 2, y);
}

static void renderWeightScreen(float weightKg, const char* statusText) {
    canvas.fillSprite(BLACK);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);

    canvas.setTextColor(GREEN);
    canvas.drawString("A:Zero", 4, 4);
    canvas.drawString("B:1kg", 78, 4);

    char weightText[32];
    snprintf(weightText, sizeof(weightText), "%.3fkg", static_cast<double>(weightKg));
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextSize(2);
    canvas.setTextColor(GREEN);
    canvas.drawString(weightText, canvas.width() / 2, 28);

    canvas.setTextSize(1);
    canvas.setTextColor(TFT_CYAN);
    if (s_outOfRange) {
        canvas.drawString("Dist: OOR", canvas.width() / 2, 56);
    } else if (s_hasDistance) {
        char distanceText[32];
        snprintf(distanceText, sizeof(distanceText), "Dist: %u mm", static_cast<unsigned>(s_lastDistanceMm));
        canvas.drawString(distanceText, canvas.width() / 2, 56);
    } else {
        canvas.drawString("Dist: waiting", canvas.width() / 2, 56);
    }

    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(WHITE);
    if (s_hasImu) {
        char accelText1[32];
        char accelText2[32];
        char gyroText1[32];
        char gyroText2[32];
        snprintf(accelText1, sizeof(accelText1), "AX %.2f AY %.2f", static_cast<double>(s_accelX),
                 static_cast<double>(s_accelY));
        snprintf(accelText2, sizeof(accelText2), "AZ %.2f", static_cast<double>(s_accelZ));
        snprintf(gyroText1, sizeof(gyroText1), "GX %.2f GY %.2f", static_cast<double>(s_gyroX),
                 static_cast<double>(s_gyroY));
        snprintf(gyroText2, sizeof(gyroText2), "GZ %.2f", static_cast<double>(s_gyroZ));
        canvas.drawString(accelText1, 4, 76);
        canvas.drawString(accelText2, 4, 90);
        canvas.drawString(gyroText1, 4, 104);
        canvas.drawString(gyroText2, 4, 118);
    } else {
        canvas.drawString("ACC/GYR: waiting", 4, 96);
    }

    if (statusText != nullptr) {
        canvas.setTextSize(1);
        canvas.setTextColor(TFT_YELLOW);
        canvas.setTextDatum(MC_DATUM);
        canvas.drawString(statusText, canvas.width() / 2, 132);
    }

    canvas.pushSprite(0, 0);
}

static void renderMessage(const char* line1, const char* line2 = nullptr) {
    canvas.fillSprite(BLACK);
    canvas.setTextDatum(MC_DATUM);
    drawCenteredText(line1, 44, 1, GREEN);
    if (line2 != nullptr) {
        drawCenteredText(line2, 76, 1, WHITE);
    }
    canvas.pushSprite(0, 0);
}

static void updateImu() {
    if (M5.Imu.isEnabled() && M5.Imu.update()) {
        M5.Imu.getAccel(&s_accelX, &s_accelY, &s_accelZ);
        M5.Imu.getGyro(&s_gyroX, &s_gyroY, &s_gyroZ);
        s_hasImu = true;
    }
}

static bool captureAverageRaw(uint32_t durationMs, const char* line1, const char* line2, long* averageRaw, long* minRaw,
                              long* maxRaw) {
    const uint32_t startMs = millis();
    int64_t rawSum = 0;
    uint32_t rawCount = 0;
    long localMinRaw = LONG_MAX;
    long localMaxRaw = LONG_MIN;

    while (millis() - startMs < durationMs) {
        M5.update();
        updateLidar();
        updateImu();

        const uint32_t elapsedMs = millis() - startMs;
        char progressText[32];
        snprintf(progressText, sizeof(progressText), "%s %lus", line2,
                 static_cast<unsigned long>((elapsedMs / 1000UL) + 1UL));
        renderMessage(line1, progressText);

        if (!scale.wait_ready_timeout(kReadyTimeoutMs)) {
            continue;
        }

        const long raw = scale.read_average(1);
        rawSum += raw;
        rawCount++;
        if (raw < localMinRaw) {
            localMinRaw = raw;
        }
        if (raw > localMaxRaw) {
            localMaxRaw = raw;
        }
    }

    if (rawCount == 0) {
        return false;
    }

    *averageRaw = static_cast<long>(rawSum / static_cast<int64_t>(rawCount));
    if (minRaw != nullptr) {
        *minRaw = localMinRaw;
    }
    if (maxRaw != nullptr) {
        *maxRaw = localMaxRaw;
    }
    return true;
}

static void updateHx711() {
    if (!s_scaleReady) {
        return;
    }

    const uint32_t now = millis();
    if (now - s_lastHx711PollMs < kHx711PollIntervalMs) {
        return;
    }
    s_lastHx711PollMs = now;

    if (!scale.is_ready()) {
        return;
    }

    const long raw = scale.read_average(1);
    const long offset = scale.get_offset();
    const long delta = raw - offset;
    float weightKg = static_cast<float>(delta) / scale.get_scale() / 1000.0f;
    if (s_hasZeroReference && raw >= s_savedZeroBandMin && raw <= s_savedZeroBandMax) {
        weightKg = 0.0f;
    }

    s_lastWeightRaw = raw;
    s_lastWeightOffset = offset;
    s_lastWeightDelta = delta;
    if (!s_hasWeightSample) {
        s_lastWeightKg = weightKg;
        s_hasWeightSample = true;
    } else {
        s_lastWeightKg = (kWeightEmaAlpha * weightKg) + ((1.0f - kWeightEmaAlpha) * s_lastWeightKg);
    }
    s_displayWeightKg = applyDisplayedZeroDeadband(s_lastWeightKg);
}

static bool captureZeroReference() {
    long averageRaw = 0;
    long minRaw = 0;
    long maxRaw = 0;
    if (!captureAverageRaw(kCaptureDurationMs, "Capturing zero...", "empty", &averageRaw, &minRaw, &maxRaw)) {
        return false;
    }

    scale.set_offset(averageRaw);
    s_savedOffset = averageRaw;
    s_savedZeroBandMin = minRaw;
    s_savedZeroBandMax = maxRaw;
    s_hasZeroReference = true;
    saveCalibration();
    Serial.printf("Zero captured: offset=%ld zero_band=[%ld,%ld]\n", averageRaw, minRaw, maxRaw);
    renderMessage("Zero captured", "Range saved");
    delay(700);
    return true;
}

static bool captureOneKilogramReference() {
    if (!s_hasZeroReference) {
        renderMessage("Set zero first", "Hold BtnA 5s");
        delay(700);
        return false;
    }

    long averageRaw = 0;
    if (!captureAverageRaw(kCaptureDurationMs, "Capturing 1kg...", "keep 1kg stable", &averageRaw, nullptr, nullptr)) {
        return false;
    }

    const long deltaRaw = averageRaw - scale.get_offset();
    if (deltaRaw == 0) {
        return false;
    }

    const float newScale = static_cast<float>(deltaRaw) / 1000.0f;
    scale.set_scale(newScale);
    s_savedScale = newScale;
    s_savedOffset = scale.get_offset();
    saveCalibration();
    Serial.printf("1kg reference captured: raw=%ld offset=%ld delta=%ld scale=%f\n", averageRaw, scale.get_offset(),
                  deltaRaw, static_cast<double>(newScale));
    renderMessage("1kg captured", "Saved");
    delay(700);
    return true;
}

static bool feedLidarFrame(uint8_t byteValue, uint16_t* distanceMm) {
    if (s_lidarFrameIdx == 0) {
        if (byteValue == 0x5C) {
            s_lidarFrame[0] = byteValue;
            s_lidarFrameIdx = 1;
        }
        return false;
    }

    s_lidarFrame[s_lidarFrameIdx++] = byteValue;
    if (s_lidarFrameIdx < 4) {
        return false;
    }

    s_lidarFrameIdx = 0;
    const uint8_t low = s_lidarFrame[1];
    const uint8_t high = s_lidarFrame[2];
    const uint8_t checksum = s_lidarFrame[3];
    const uint8_t expected = static_cast<uint8_t>(~(low + high));
    if (checksum != expected) {
        return false;
    }

    *distanceMm = static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
    return true;
}

static void updateLidar() {
    while (Serial2.available() > 0) {
        uint16_t distanceMm = 0;
        if (!feedLidarFrame(static_cast<uint8_t>(Serial2.read()), &distanceMm)) {
            continue;
        }

        if (distanceMm == 0xFFFF) {
            s_outOfRange = true;
            s_hasDistance = false;
        } else {
            s_lastDistanceMm = distanceMm;
            s_hasDistance = true;
            s_outOfRange = false;
        }
    }
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setRotation(1);
    canvas.setPsram(false);
    canvas.setColorDepth(8);
    canvas.createSprite(M5.Display.width(), M5.Display.height());
    canvas.setTextDatum(MC_DATUM);

    renderMessage("HX711 init...", "M5StickC Plus2");
    Serial.println();
    Serial.println("HX711 + LiDAR reader starting...");
    Serial.printf("hx711 pins: dout=%d sck=%d default_scale=%.3f\n", kLoadCellDoutPin, kLoadCellSckPin,
                  static_cast<double>(kDefaultScale));
    Serial.printf("lidar pins: rx=%d tx=%d baud=%u\n", kLidarRxPin, kLidarTxPin, static_cast<unsigned>(kLidarBaud));

    preferences.begin("stirrup1", false);
    loadCalibration();

    scale.begin(kLoadCellDoutPin, kLoadCellSckPin);
    scale.set_scale(s_savedScale);
    s_scaleReady = scale.wait_ready_timeout(500);
    Serial2.setRxBufferSize(2048);
    Serial2.begin(kLidarBaud, SERIAL_8N1, kLidarRxPin, kLidarTxPin);

    if (!s_scaleReady) {
        Serial.println("HX711 not ready. Check wiring/power.");
        renderMessage("HX711 not ready", "Check DAT/CLK/power");
        return;
    }

    if (s_hasZeroReference) {
        scale.set_offset(s_savedOffset);
        Serial.printf("Loaded calibration: offset=%ld scale=%f\n", s_savedOffset, static_cast<double>(s_savedScale));
        renderMessage("Calibration loaded", "Ready");
    } else {
        Serial.println("Hold BtnA for 5s to capture zero.");
        Serial.println("Hold BtnB for 5s with 1kg to set scale.");
        renderMessage("Hold BtnA 5s", "empty -> zero");
    }
    delay(600);
}

void loop() {
    M5.update();
    updateLidar();

    if (!s_scaleReady) {
        renderMessage("HX711 not ready", "Check DAT/CLK/power");
        delay(100);
        return;
    }

    const uint32_t now = millis();
    if (now - s_lastImuUpdateMs >= kImuIntervalMs) {
        s_lastImuUpdateMs = now;
        updateImu();
    }
    updateHx711();

    if (M5.BtnA.wasReleased()) {
        s_btnAHoldHandled = false;
    }
    if (M5.BtnB.wasReleased()) {
        s_btnBHoldHandled = false;
    }

    if (M5.BtnA.pressedFor(kHoldToCaptureMs) && !s_btnAHoldHandled) {
        s_btnAHoldHandled = true;
        if (!captureZeroReference()) {
            Serial.println("Zero capture failed.");
            renderMessage("Zero failed", "Try again");
            delay(700);
        }
    }

    if (M5.BtnB.pressedFor(kHoldToCaptureMs) && !s_btnBHoldHandled) {
        s_btnBHoldHandled = true;
        if (!captureOneKilogramReference()) {
            Serial.println("1kg capture failed.");
            renderMessage("1kg failed", "Try again");
            delay(700);
        }
    }

    if (now - s_lastOutputMs < kOutputIntervalMs) {
        return;
    }
    s_lastOutputMs = now;

    char distanceText[16];
    if (s_outOfRange) {
        snprintf(distanceText, sizeof(distanceText), "OOR");
    } else if (s_hasDistance) {
        snprintf(distanceText, sizeof(distanceText), "%u", static_cast<unsigned>(s_lastDistanceMm));
    } else {
        snprintf(distanceText, sizeof(distanceText), "wait");
    }

    if (s_hasImu) {
        Serial.printf(
            "weight=%.3f kg raw=%ld offset=%ld delta=%ld scale=%f dist=%s acc=(%.3f,%.3f,%.3f) gyr=(%.3f,%.3f,%.3f)\n",
            static_cast<double>(s_hasWeightSample ? s_displayWeightKg : 0.0f), s_lastWeightRaw, s_lastWeightOffset,
            s_lastWeightDelta, static_cast<double>(scale.get_scale()), distanceText, static_cast<double>(s_accelX),
            static_cast<double>(s_accelY), static_cast<double>(s_accelZ),
            static_cast<double>(s_gyroX), static_cast<double>(s_gyroY), static_cast<double>(s_gyroZ));
    } else {
        Serial.printf("weight=%.3f kg raw=%ld offset=%ld delta=%ld scale=%f dist=%s imu=wait\n",
                      static_cast<double>(s_hasWeightSample ? s_displayWeightKg : 0.0f), s_lastWeightRaw,
                      s_lastWeightOffset, s_lastWeightDelta, static_cast<double>(scale.get_scale()),
                      distanceText);
    }

    renderWeightScreen(s_hasWeightSample ? s_displayWeightKg : 0.0f, nullptr);
}