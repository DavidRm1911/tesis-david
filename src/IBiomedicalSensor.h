#ifndef IBIOMEDICAL_SENSOR_H
#define IBIOMEDICAL_SENSOR_H

#include <Arduino.h>

/**
 * @brief Abstract interface for biomedical sensors (e.g., ECG, EMG, PPG)
 * 
 * Users implement this interface to provide sensor-specific reading logic.
 * The BiomedicalPipeline orchestrates continuous sampling via this interface.
 */
class IBiomedicalSensor {
public:
    virtual ~IBiomedicalSensor() = default;

    /**
     * @brief Read a single sample from the sensor
     * @return float The current sensor reading (e.g., voltage in mV for ECG)
     * 
     * Called repeatedly by BiomedicalPipeline::tick() to ingest continuous samples.
     * Must be non-blocking and return quickly (<1ms for reliable real-time performance).
     */
    virtual float readSample() = 0;

    /**
     * @brief Get the sampling rate of the sensor
     * @return int Sampling frequency in Hz (e.g., 250 for ECG, 1000 for high-precision EMG)
     * 
     * Typically constant for a given sensor. Used for diagnostics and timing calculations.
     */
    virtual int getSamplingRate() = 0;

    /**
     * @brief Get a unique identifier for the sensor
     * @return String Sensor identifier (e.g., "AD8232_Wrist" or "SparkFun_ECG_Shield")
     * 
     * Useful for logging, multi-sensor systems, and debugging.
     */
    virtual String getSensorId() = 0;
};

#endif  // IBIOMEDICAL_SENSOR_H
