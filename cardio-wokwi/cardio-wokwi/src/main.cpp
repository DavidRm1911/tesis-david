#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

#include "IBiomedicalSensor.h"
#include "BiomedicalPipeline.h"
#include "secrets.h"

// ── Pines ─────────────────────────────────────────────────────
#define LED_R     25
#define LED_G     26
#define LED_B     27
#define SDA_PIN   21
#define SCL_PIN   22

// ── WiFi / AWS IoT ────────────────────────────────────────────
#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define TOPIC_ECG     "cardio/ecg/P001"
#define TOPIC_ALERT   "cardio/alert/P001"

// ── OLED ──────────────────────────────────────────────────────
#define SCREEN_W 128
#define SCREEN_H  64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
bool oledOk = false;

// ── MQTT ──────────────────────────────────────────────────────
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttOk = false;

// ── Helpers LED RGB ───────────────────────────────────────────
void rgbSet(bool r, bool g, bool b) {
    digitalWrite(LED_R, r);
    digitalWrite(LED_G, g);
    digitalWrite(LED_B, b);
}

// ── MockAD8232Sensor ──────────────────────────────────────────
class MockAD8232Sensor : public IBiomedicalSensor {
private:
    int samplingRate;
    unsigned long sampleCount;
    float noiseAmplitude;

    float gaussianNoise() {
        static bool hasSpare = false;
        static float spare;
        if (hasSpare) { hasSpare = false; return spare; }
        hasSpare = true;
        float u1 = max(random(1000) / 1000.0f, 1e-10f);
        float u2 = random(1000) / 1000.0f;
        float mag = sqrtf(-2.0f * logf(u1));
        spare = mag * sinf(2.0f * M_PI * u2);
        return mag * cosf(2.0f * M_PI * u2);
    }

public:
    MockAD8232Sensor(int sr = 250, float noise = 0.05f)
        : samplingRate(sr), sampleCount(0), noiseAmplitude(noise) {
        randomSeed(42);
    }

    float readSample() override {
        float t = sampleCount++ / (float)samplingRate;
        return 0.5f * sinf(2.0f * M_PI * 1.0f  * t)
             + 1.2f * sinf(2.0f * M_PI * 2.0f  * t + 1.5f)
             + 0.6f * sinf(2.0f * M_PI * 3.0f  * t + 3.0f)
             + 0.3f * sinf(2.0f * M_PI * 10.0f * t)
             + noiseAmplitude * gaussianNoise();
    }

    int getSamplingRate() override { return samplingRate; }
    String getSensorId() override  { return "AD8232_Mock"; }
};

// ── CardioStreamApp ───────────────────────────────────────────
class CardioStreamApp : public BiomedicalPipeline {
private:
    int windowCount = 0;

    float computeMean(float* d, int n) {
        float s = 0; for (int i = 0; i < n; i++) s += d[i]; return s / n;
    }
    float computeStdDev(float* d, int n) {
        float m = computeMean(d, n), s = 0;
        for (int i = 0; i < n; i++) s += (d[i] - m) * (d[i] - m);
        return sqrtf(s / n);
    }

public:
    CardioStreamApp(IBiomedicalSensor* s) : BiomedicalPipeline(s, 256) {}

    void onWindowReady(float* window, int size) override {
        windowCount++;
        float mean   = computeMean(window, size);
        float stddev = computeStdDev(window, size);
        bool  anomaly = (stddev > 1.5f);

        // RGB: verde=normal, rojo=anomalia, amarillo=offline
        if (anomaly)       rgbSet(true,  false, false);
        else if (!mqttOk)  rgbSet(true,  true,  false);
        else               rgbSet(false, true,  false);

        // OLED
        if (oledOk) {
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println("== CardioStream ==");
            display.printf("Ventana : #%d\n",   windowCount);
            display.printf("Mean    : %.4f\n",  mean);
            display.printf("Std     : %.4f\n",  stddev);
            display.printf("Estado  : %s\n",    anomaly ? "ANOMALIA" : "Normal");
            display.printf("AWS     : %s\n",    mqttOk  ? "OK"       : "offline");
            display.display();
        }

        // Serial
        Serial.printf("[V#%d] mean=%.4f std=%.4f %s\n",
            windowCount, mean, stddev, anomaly ? "[!ANOMALIA]" : "");

        // MQTT
        if (mqttOk && mqttClient.connected()) {
            JsonDocument doc;
            doc["patientId"]   = "P001";
            doc["windowIndex"] = windowCount;
            doc["mean"]        = mean;
            doc["stddev"]      = stddev;
            doc["anomaly"]     = anomaly;
            doc["timestamp"]   = millis();
            char payload[256];
            serializeJson(doc, payload);
            mqttClient.publish(TOPIC_ECG, payload);
        }
    }
};

// ── Callback MQTT ─────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
    Serial.printf("[MQTT] %s: %s\n", topic, msg.c_str());
}

// ── Setup ─────────────────────────────────────────────────────
CardioStreamApp* app = nullptr;

void setup() {
    Serial.begin(115200);

    // LED RGB — parpadeo de inicio para confirmar firmware
    pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
    for (int i = 0; i < 3; i++) {
        rgbSet(true, true, true); delay(150);
        rgbSet(false, false, false); delay(150);
    }
    rgbSet(true, true, false); // amarillo = iniciando

    // OLED
    Wire.begin(SDA_PIN, SCL_PIN);
    oledOk = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    Serial.printf("\n=== CardioStream Fase 1B === | OLED: %s\n", oledOk ? "OK" : "FAIL");
    if (oledOk) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("CardioStream v1B");
        display.println("Conectando WiFi...");
        display.display();
    }

    // Private Gateway provee DNS via 10.13.37.1 — no sobreescribir con 8.8.8.8
    Serial.print("WiFi: conectando");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500); Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi OK | IP: %s | DNS: %s\n",
            WiFi.localIP().toString().c_str(),
            WiFi.dnsIP().toString().c_str());

        // Hora: NTP primero, si falla usa timestamp de compilacion (siempre reciente)
        configTzTime("<-05>5", "pool.ntp.org", "time.nist.gov");
        struct tm timeinfo;
        bool ntpOk = false;
        for (int i = 0; i < 10 && !ntpOk; i++) {
            delay(500); ntpOk = getLocalTime(&timeinfo, 100);
        }
        if (!ntpOk) {
            struct timeval tv = { BUILD_TIMESTAMP, 0 };
            settimeofday(&tv, nullptr);
            setenv("TZ", "<-05>5", 1); tzset();
            getLocalTime(&timeinfo, 100);
        }
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%d/%m/%Y %H:%M:%S", &timeinfo);
        Serial.printf("Hora Lima: %s %s\n", timebuf, ntpOk ? "(NTP)" : "(build)");

        // mTLS: puerto 443 + ALPN evita bloqueos de 8883 en redes/simulador
        static const char* alpn[] = { "x-amzn-mqtt-ca", NULL };
        wifiClient.setAlpnProtocols(alpn);
        wifiClient.setCACert(AWS_CERT_CA);
        wifiClient.setCertificate(AWS_CERT_CRT);
        wifiClient.setPrivateKey(AWS_CERT_PRIVATE);
        mqttClient.setServer(AWS_IOT_ENDPOINT, 443);
        mqttClient.setCallback(mqttCallback);
        mqttClient.setBufferSize(512);

        Serial.print("MQTT: conectando");
        for (int i = 0; i < 3; i++) {
            if (mqttClient.connect(THINGNAME)) {
                mqttClient.subscribe(TOPIC_ALERT);
                mqttOk = true;
                Serial.println(" OK");
                break;
            }
            Serial.printf(".\n  estado=%d\n", mqttClient.state());
            delay(1000);
        }
        if (!mqttOk) Serial.println(" offline");
    } else {
        Serial.println("\nWiFi offline");
    }

    if (oledOk) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "OK" : "offline");
        display.printf("AWS : %s\n", mqttOk ? "OK" : "offline");
        display.println("\nIniciando pipeline...");
        display.display();
    }

    app = new CardioStreamApp(new MockAD8232Sensor(250, 0.05f));
    Serial.printf("Sensor: %s | %dHz | ventana: %d\n",
        app->getSensor()->getSensorId().c_str(),
        app->getSensor()->getSamplingRate(),
        app->getWindowSize());
    Serial.println("Pipeline iniciado.\n");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    if (mqttOk) mqttClient.loop();
    if (app) {
        app->tick();
        delayMicroseconds(4000);
    }
}
