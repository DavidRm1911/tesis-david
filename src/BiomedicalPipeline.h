#ifndef BIOMEDICAL_PIPELINE_H
#define BIOMEDICAL_PIPELINE_H

#include "IBiomedicalSensor.h"
#include "SignalBuffer.h"
#include "WindowExtractor.h"

/**
 * @brief Orchestrator for continuous biomedical signal processing
 * 
 * Connects a sensor, buffers samples, extracts windows, and invokes
 * callbacks when windows are ready. Users inherit from this class
 * and override onWindowReady() to implement custom signal processing
 * (e.g., feature extraction, cloud transmission, anomaly detection).
 * 
 * Pipeline Model (vs. CQRS):
 *   - Optimized for CONTINUOUS time-series data (ECG, EMG, PPG)
 *   - Samples flow: Sensor → Buffer → Window Extractor → Callback
 *   - No discrete events; natural sample-by-sample progression
 *   - Reduces latency vs. CQRS (which separates commands/queries for discrete events)
 * 
 * Usage Example:
 * @code
 *   class MyECGApp : public BiomedicalPipeline {
 *   public:
 *       MyECGApp(IBiomedicalSensor* sensor) 
 *           : BiomedicalPipeline(sensor, 256) {}  // 256-sample windows
 *       
 *       void onWindowReady(float* window, int size) override {
 *           // Process window: compute features, send to cloud, etc.
 *           float mean = computeMean(window, size);
 *           Serial.println(mean);
 *       }
 *   };
 *   
 *   void setup() {
 *       MyECGApp* app = new MyECGApp(new MockAD8232Sensor());
 *   }
 *   
 *   void loop() {
 *       app->tick();  // Call repeatedly; triggers onWindowReady() as windows arrive
 *   }
 * @endcode
 */
class BiomedicalPipeline {
protected:
    IBiomedicalSensor* sensor;      ///< Pointer to sensor implementation
    SignalBuffer<float> buffer;     ///< Internal ring buffer
    WindowExtractor extractor;      ///< Window extraction dispatcher

public:
    /**
     * @brief Construct the biomedical pipeline
     * @param sensor Pointer to IBiomedicalSensor implementation
     * @param windowSize Number of samples per window (e.g., 256)
     * @param stepSize Number of new samples between extractions
     *                  Default: windowSize (non-overlapping windows)
     * 
     * Initializes internal ring buffer with windowSize capacity.
     * Users should inherit from this class and override onWindowReady().
     */
    BiomedicalPipeline(IBiomedicalSensor* sensor, int windowSize, int stepSize = -1)
        : sensor(sensor),
          buffer(windowSize),
          extractor(windowSize, stepSize == -1 ? windowSize : stepSize) {}

    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~BiomedicalPipeline() = default;

    /**
     * @brief Core processing loop - call this from Arduino loop()
     * 
     * Performs the following steps:
     *   1. Reads one sample from the sensor
     *   2. Pushes sample to the circular buffer
     *   3. Attempts to extract a window (based on step size)
     *   4. If window ready: calls onWindowReady(window, size)
     * 
     * This method is non-blocking and runs in O(1) time per call.
     * Safe to call repeatedly and rapidly (e.g., every 4ms for 250Hz ECG).
     */
    void tick() {
        // Step 1: Read sample from sensor
        float sample = sensor->readSample();

        // Step 2: Push to buffer
        buffer.push(sample);

        // Step 3 & 4: Try to extract window and invoke callback
        float window[buffer.getMaxSize()];
        if (extractor.extract(buffer, window)) {
            onWindowReady(window, extractor.getWindowSize());
        }
    }

    /**
     * @brief Callback invoked when a new window is ready
     * @param window Pointer to array of samples (size = windowSize)
     * @param size Number of samples in the window
     * 
     * OVERRIDE THIS METHOD in derived classes to implement custom processing.
     * Examples:
     *   - Compute heart rate from ECG window
     *   - Extract features (RMS, entropy, frequency domain)
     *   - Publish to cloud/database
     *   - Update UI with live signal data
     * 
     * This is called from tick(), so keep it quick (<10ms) to avoid blocking
     * the real-time sampling loop.
     */
    virtual void onWindowReady(float* window, int size) = 0;

    /**
     * @brief Get pointer to the underlying sensor
     * @return IBiomedicalSensor* Sensor being used by this pipeline
     */
    IBiomedicalSensor* getSensor() const {
        return sensor;
    }

    /**
     * @brief Get the current number of samples in the buffer
     * @return int Buffer occupancy (0 to windowSize)
     */
    int getBufferSize() const {
        return buffer.size();
    }

    /**
     * @brief Get the window size
     * @return int Samples per window
     */
    int getWindowSize() const {
        return extractor.getWindowSize();
    }

    /**
     * @brief Get the step size (samples between extractions)
     * @return int Step size
     */
    int getStepSize() const {
        return extractor.getStepSize();
    }

    /**
     * @brief Check if the buffer is currently full
     * @return true if buffer has reached windowSize capacity
     */
    bool isBufferFull() const {
        return buffer.isFull();
    }

    /**
     * @brief Reset the pipeline state
     * 
     * Clears buffer and resets window extraction counter.
     * Useful when restarting signal acquisition or switching sensors.
     */
    void reset() {
        buffer.reset();
        extractor.reset();
    }
};

#endif  // BIOMEDICAL_PIPELINE_H
