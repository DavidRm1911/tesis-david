#ifndef SIGNAL_BUFFER_H
#define SIGNAL_BUFFER_H

/**
 * @brief Circular ring buffer for continuous signal data
 * 
 * Stores a fixed number of samples in a circular manner. When full,
 * new samples overwrite the oldest data. Designed for low-latency,
 * real-time signal processing without memory fragmentation.
 * 
 * @tparam T Data type (default: float for biomedical signals)
 */
template<typename T = float>
class SignalBuffer {
private:
    T* buffer;              ///< Circular array of samples
    int maxSize;            ///< Capacity of the buffer
    int head;               ///< Write pointer (next position to write)
    int count;              ///< Current number of samples (0 to maxSize)

public:
    /**
     * @brief Construct a circular signal buffer
     * @param size Maximum number of samples to store
     */
    explicit SignalBuffer(int size) 
        : maxSize(size), head(0), count(0) {
        buffer = new T[maxSize];
    }

    /**
     * @brief Destructor - free allocated memory
     */
    ~SignalBuffer() {
        delete[] buffer;
    }

    /**
     * @brief Add a sample to the buffer
     * @param value The sample value to store
     * 
     * If buffer is full, overwrites the oldest sample (circular behavior).
     * Runs in O(1) constant time.
     */
    void push(T value) {
        buffer[head] = value;
        head = (head + 1) % maxSize;
        if (count < maxSize) {
            count++;
        }
    }

    /**
     * @brief Extract the last N samples from the buffer
     * @param out Array to store the output window (must have capacity >= size)
     * @param size Number of samples to extract
     * @return true if buffer contains at least 'size' samples; false otherwise
     * 
     * Copies the most recent 'size' samples in chronological order into 'out'.
     * If buffer is not yet full with 'size' samples, returns false without modifying 'out'.
     */
    bool getWindow(T* out, int size) {
        if (count < size) {
            return false;
        }

        // Calculate the index of the oldest sample we want to include
        int startIdx = (head - count + maxSize) % maxSize;
        int endIdx = (head - 1 + maxSize) % maxSize;
        
        // We want the last 'size' samples
        startIdx = (head - size + maxSize) % maxSize;

        // Copy samples in order: oldest to newest
        for (int i = 0; i < size; i++) {
            int idx = (startIdx + i) % maxSize;
            out[i] = buffer[idx];
        }

        return true;
    }

    /**
     * @brief Check if the buffer is full
     * @return true if buffer contains maxSize samples; false otherwise
     */
    bool isFull() const {
        return count == maxSize;
    }

    /**
     * @brief Get the current number of samples in the buffer
     * @return int Number of samples (0 to maxSize)
     */
    int size() const {
        return count;
    }

    /**
     * @brief Get the maximum capacity of the buffer
     * @return int Maximum number of samples the buffer can hold
     */
    int getMaxSize() const {
        return maxSize;
    }

    /**
     * @brief Reset the buffer to empty state
     * 
     * Clears the count but does not deallocate memory.
     */
    void reset() {
        head = 0;
        count = 0;
    }
};

#endif  // SIGNAL_BUFFER_H
