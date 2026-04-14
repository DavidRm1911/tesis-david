#ifndef WINDOW_EXTRACTOR_H
#define WINDOW_EXTRACTOR_H

#include "SignalBuffer.h"

/**
 * @brief Extracts fixed-size windows from a continuous signal stream
 * 
 * Tracks sample progression and determines when a new window of N samples
 * is ready for extraction from the signal buffer. Supports sliding windows
 * with configurable step size.
 * 
 * Example:
 *   - windowSize=256, stepSize=128 (50% overlap)
 *   - Extracts new window every 128 new samples
 *   - Each window contains 256 samples
 */
class WindowExtractor {
private:
    int windowSize;         ///< Number of samples per window
    int stepSize;           ///< Number of new samples between window extractions
    int sampleCounter;      ///< Count of samples since last extraction

public:
    /**
     * @brief Construct a window extractor
     * @param windowSize Number of samples per window (e.g., 256)
     * @param stepSize Number of new samples before next extraction
     *                 Default: windowSize (non-overlapping windows)
     */
    WindowExtractor(int windowSize, int stepSize = -1)
        : windowSize(windowSize), 
          stepSize(stepSize == -1 ? windowSize : stepSize),
          sampleCounter(0) {
        // Validate parameters
        if (this->stepSize <= 0 || this->stepSize > windowSize) {
            this->stepSize = windowSize;
        }
    }

    /**
     * @brief Attempt to extract a window from the buffer
     * @param buf Reference to the SignalBuffer
     * @param window Array to store the extracted window (must have capacity >= windowSize)
     * @return true if a new window was extracted; output written to 'window'
     *         false if not enough new samples collected yet
     * 
     * Call this after each push() to the buffer. It will return true at regular
     * intervals (every stepSize samples) and fill 'window' with the latest
     * windowSize samples from the buffer.
     */
    bool extract(SignalBuffer<float>& buf, float* window) {
        sampleCounter++;

        // Check if we've collected enough samples for a new window
        if (sampleCounter >= stepSize && buf.size() >= windowSize) {
            // Extract the window
            bool success = buf.getWindow(window, windowSize);
            if (success) {
                sampleCounter = 0;  // Reset counter for next window
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Get the window size
     * @return int Number of samples per window
     */
    int getWindowSize() const {
        return windowSize;
    }

    /**
     * @brief Get the step size (samples between extractions)
     * @return int Step size in samples
     */
    int getStepSize() const {
        return stepSize;
    }

    /**
     * @brief Get the current sample counter (samples since last extraction)
     * @return int Current counter value (0 to stepSize)
     */
    int getSampleCounter() const {
        return sampleCounter;
    }

    /**
     * @brief Reset the extractor to initial state
     * 
     * Useful when restarting signal processing or switching sensors.
     */
    void reset() {
        sampleCounter = 0;
    }
};

#endif  // WINDOW_EXTRACTOR_H
