#include <Arduino.h>
#include "../src/IBiomedicalSensor.h"
#include "../src/BiomedicalPipeline.h"

// ============================================================================
// MockAD8232Sensor: Simulated ECG sensor
// ============================================================================

/**
 * @brief Simulated AD8232 Heart Rate Monitor ECG Sensor
 * 
 * Generates synthetic ECG waveform approximating realistic ECG signal:
 *   - Base: sine wave at ~1 Hz (60 BPM) to simulate heart rhythm
 *   - Harmonics: adds higher-frequency components for realistic morphology
 *   - Gaussian noise: simulates measurement noise (~0.1 mV)
 * 
 * Real AD8232 output: typically 0-3.3V on ESP32 ADC, ~100-250 Hz sampling.
 * This mock generates abstract ECG values (not tied to specific voltage).
 */
class MockAD8232Sensor : public IBiomedicalSensor {
private:
    int samplingRate;           ///< Hz (samples per second)
    unsigned long sampleCount;  ///< Total samples generated
    float noiseAmplitude;       ///< Gaussian noise standard deviation

    /**
     * @brief Simple linear congruential pseudorandom generator for noise
     * @return float Random value in normal distribution ~N(0,1)
     * 
     * Fast, deterministic, suitable for ESP32 embedded use.
     */
    float getGaussianNoise() {
        // Box-Muller transform: two uniform randoms → two normal randoms
        static bool hasSpare = false;
        static float spare;

        if (hasSpare) {
            hasSpare = false;
            return spare;
        }

        hasSpare = true;

        // Generate two uniform random numbers [0,1)
        float u1 = random(1000) / 1000.0f;
        float u2 = random(1000) / 1000.0f;

        // Avoid log(0)
        if (u1 < 1e-10f) u1 = 1e-10f;

        // Box-Muller transformation
        float mag = sqrtf(-2.0f * logf(u1));
        float z0 = mag * cosf(2.0f * M_PI * u2);
        spare = mag * sinf(2.0f * M_PI * u2);

        return z0;
    }

public:
    /**
     * @brief Construct mock ECG sensor
     * @param samplingRate Desired sampling rate (Hz)
     * @param noiseAmplitude Standard deviation of Gaussian noise
     */
    MockAD8232Sensor(int samplingRate = 250, float noiseAmplitude = 0.1f)
        : samplingRate(samplingRate),
          sampleCount(0),
          noiseAmplitude(noiseAmplitude) {
        randomSeed(analogRead(A0));  // Seed pseudo-RNG
    }

    /**
     * @brief Generate next ECG sample
     * @return float Synthetic ECG reading
     * 
     * Approximates realistic ECG morphology:
     *   - P wave (atrial depolarization)
     *   - QRS complex (ventricular depolarization)
     *   - T wave (ventricular repolarization)
     * 
     * Modeled as sum of sine waves + noise, ~60 BPM heart rate.
     */
    float readSample() override {
        // Time in seconds since start
        float t = sampleCount / (float)samplingRate;

        // Heart rate: ~60 BPM → ~1 Hz fundamental
        float heartRateHz = 1.0f;

        // ECG approximation: sum of harmonic components
        // - Fundamental: main rhythm
        // - 2nd & 3rd harmonics: add QRS sharpness and complexity
        float ecg = 0.0f;

        // Fundamental (P-wave like)
        ecg += 0.5f * sinf(2.0f * M_PI * heartRateHz * t);

        // 2nd harmonic (QRS complex - high amplitude)
        ecg += 1.2f * sinf(2.0f * M_PI * 2.0f * heartRateHz * t + 1.5f);

        // 3rd harmonic (T-wave like detail)
        ecg += 0.6f * sinf(2.0f * M_PI * 3.0f * heartRateHz * t + 3.0f);

        // Add high-frequency content (simulates sensor noise and muscle artifact)
        ecg += 0.3f * sinf(2.0f * M_PI * 10.0f * t);

        // Add Gaussian noise
        ecg += noiseAmplitude * getGaussianNoise();

        sampleCount++;
        return ecg;
    }

    /**
     * @brief Get sampling rate
     * @return int Samples per second
     */
    int getSamplingRate() override {
        return samplingRate;
    }

    /**
     * @brief Get sensor identifier
     * @return String Sensor name
     */
    String getSensorId() override {
        return "AD8232_Simulated";
    }

    /**
     * @brief Get total samples generated (for testing/diagnostics)
     * @return unsigned long Sample count
     */
    unsigned long getTotalSamples() const {
        return sampleCount;
    }
};

// ============================================================================
// CardioStreamApp: ECG Processing Application
// ============================================================================

/**
 * @brief Example ECG processing app inheriting from BiomedicalPipeline
 * 
 * Implements onWindowReady() to:
 *   - Compute window statistics (mean, min, max, variance)
 *   - Print to Serial for monitoring
 *   - Could be extended with: heart rate estimation, arrhythmia detection, cloud sync
 */
class CardioStreamApp : public BiomedicalPipeline {
private:
    unsigned long windowCount;  ///< Number of windows processed

    /**
     * @brief Helper: compute mean of array
     */
    float computeMean(float* data, int size) {
        float sum = 0.0f;
        for (int i = 0; i < size; i++) {
            sum += data[i];
        }
        return sum / size;
    }

    /**
     * @brief Helper: compute variance of array
     */
    float computeVariance(float* data, int size) {
        float mean = computeMean(data, size);
        float sumSqDiff = 0.0f;
        for (int i = 0; i < size; i++) {
            float diff = data[i] - mean;
            sumSqDiff += diff * diff;
        }
        return sumSqDiff / size;
    }

    /**
     * @brief Helper: find min value
     */
    float findMin(float* data, int size) {
        float minVal = data[0];
        for (int i = 1; i < size; i++) {
            if (data[i] < minVal) minVal = data[i];
        }
        return minVal;
    }

    /**
     * @brief Helper: find max value
     */
    float findMax(float* data, int size) {
        float maxVal = data[0];
        for (int i = 1; i < size; i++) {
            if (data[i] > maxVal) maxVal = data[i];
        }
        return maxVal;
    }

public:
    /**
     * @brief Construct CardioStream app
     * @param sensor Pointer to IBiomedicalSensor (e.g., MockAD8232Sensor)
     */
    CardioStreamApp(IBiomedicalSensor* sensor)
        : BiomedicalPipeline(sensor, 256),  // 256 samples @ 250Hz ≈ 1 second window
          windowCount(0) {}

    /**
     * @brief Get number of windows processed so far
     * @return unsigned long Window count
     */
    unsigned long getWindowCount() const {
        return windowCount;
    }

    /**
     * @brief Callback: invoked when a new ECG window is ready
     * @param window Pointer to 256 ECG samples
     * @param size Always 256 for this app
     * 
     * Computes and prints statistics to Serial. In a real application,
     * this could send data to cloud, update display, or trigger alerts.
     */
    void onWindowReady(float* window, int size) override {
        windowCount++;

        // Compute statistics
        float mean = computeMean(window, size);
        float variance = computeVariance(window, size);
        float minVal = findMin(window, size);
        float maxVal = findMax(window, size);
        float stdDev = sqrtf(variance);

        // Print to Serial monitor
        Serial.print("[Window #");
        Serial.print(windowCount);
        Serial.print("] Samples: ");
        Serial.print(size);
        Serial.print(", Mean: ");
        Serial.print(mean, 4);
        Serial.print(", StdDev: ");
        Serial.print(stdDev, 4);
        Serial.print(", Min: ");
        Serial.print(minVal, 4);
        Serial.print(", Max: ");
        Serial.print(maxVal, 4);
        Serial.println();
    }
};

// ============================================================================
// Setup & Loop
// ============================================================================

CardioStreamApp* cardioApp = nullptr;

/**
 * @brief Arduino setup() - Initialize sensor and pipeline
 */
void setup() {
    Serial.begin(115200);
    delay(1000);  // Give Serial monitor time to connect

    Serial.println("\n========================================");
    Serial.println("CardioStream IoT Framework - ECG Demo");
    Serial.println("========================================\n");

    // Create mock sensor (250 Hz, ~0.1 mV noise)
    IBiomedicalSensor* sensor = new MockAD8232Sensor(250, 0.1f);

    // Create ECG app
    cardioApp = new CardioStreamApp(sensor);

    // Print configuration
    Serial.print("Sensor: ");
    Serial.println(cardioApp->getSensor()->getSensorId());
    Serial.print("Sampling Rate: ");
    Serial.print(cardioApp->getSensor()->getSamplingRate());
    Serial.println(" Hz");
    Serial.print("Window Size: ");
    Serial.print(cardioApp->getWindowSize());
    Serial.println(" samples");
    Serial.print("Window Duration: ~");
    Serial.print((float)cardioApp->getWindowSize() / cardioApp->getSensor()->getSamplingRate());
    Serial.println(" seconds");
    Serial.println("\nStarting acquisition...\n");
}

/**
 * @brief Arduino loop() - Process continuous ECG stream
 * 
 * This is called repeatedly by the ESP32. Each call processes one sample.
 * At 250 Hz sampling rate with delayMicroseconds(4000), we get ~250 samples/second.
 */
void loop() {
    if (cardioApp != nullptr) {
        // Process one sample: read sensor → buffer → extract window → callback
        cardioApp->tick();

        // Simulate realtime sampling: 4ms delay ≈ 250 Hz
        // In real hardware: ADC interrupt would drive sampling instead of delay
        delayMicroseconds(4000);

        // Stop after 10 windows for demo (can remove for continuous operation)
        if (cardioApp->getWindowCount() >= 10) {
            Serial.println("\n========================================");
            Serial.println("Demo complete. Processed 10 windows.");
            Serial.println("========================================");
            while (true) {
                delay(1000);  // Halt
            }
        }
    }
}
