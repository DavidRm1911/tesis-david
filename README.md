# CardioStream IoT Framework

**Release v1.0** | C++ Framework for Continuous Biomedical Signal Processing on ESP32

---

## Overview

**CardioStream IoT Framework** is a lightweight, real-time C++ framework for processing continuous biomedical signals (ECG, EMG, PPG, etc.) on embedded systems like the **ESP32**. It provides a clean pipeline architecture for ingesting sensor data, buffering samples, extracting analysis windows, and triggering callbacks for custom signal processing.

### Key Characteristics

- ✅ **Continuous Signal Stream Processing** — optimized for real-time ECG/EMG/PPG data
- ✅ **Memory Efficient** — circular ring buffer prevents fragmentation
- ✅ **Non-Blocking** — all operations run in O(1) constant time
- ✅ **Hardware-Agnostic Interface** — implement `IBiomedicalSensor` for any sensor
- ✅ **Virtual Callback Pattern** — inherit `BiomedicalPipeline` and override `onWindowReady()`
- ✅ **Template-Based Buffer** — supports any numeric type (float, int, double)

### Use Cases

- 📊 Heart rate monitoring (ECG-based wearables)
- 💪 Muscle fatigue detection (EMG analysis)
- 🩸 Blood oxygen monitoring (PPG signals)
- 🧠 Neurotechnology applications (EEG/BCI)
- ⏱️ Real-time feature extraction from time-series data

---

## Architecture

### Why Pipeline, Not CQRS?

**CQRS (Command Query Responsibility Segregation)** separates commands (state changes) from queries (reads), with emphasis on **discrete, event-driven** systems. Biomedical signal processing is fundamentally different:

| Aspect | CQRS | CardioStream Pipeline |
|--------|------|----------------------|
| **Nature** | Discrete events (user action, button press) | Continuous stream (ECG samples @ 250 Hz) |
| **Unit of Work** | Single command/query | Stream of samples → Window of N samples |
| **Latency** | Event-driven (ms to s) | Real-time critical (<4ms/sample @ 250Hz) |
| **Buffering** | Event log or command queue | Fixed-size ring buffer (circular) |
| **Concurrency Model** | Read/Write separation | Sample-by-sample ingestion → callback dispatch |
| **Overhead** | Event serialization, routing | O(1) per sample, minimal allocation |

**Pipeline advantages for continuous signals:**

1. **Natural Progression** — Sample → Buffer → Window → Callback mirrors real biomedical acquisition
2. **Zero-Copy Geometry** — Ring buffer avoids malloc/free per sample; crucial for embedded systems
3. **Predictable Latency** — O(1) per tick() guaranteed; no event routing overhead
4. **Windowing Native** — Signal analysis typically works on fixed windows (256 samples @ 250Hz = 1 second); CQRS would fragment into 256 events

---

## Class Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│ IBiomedicalSensor (Abstract Interface)                          │
├─────────────────────────────────────────────────────────────────┤
│ + readSample() : float                                          │
│ + getSamplingRate() : int                                       │
│ + getSensorId() : String                                        │
└──────────────────────┬──────────────────────────────────────────┘
                       │ implements
                       │
        ┌──────────────┴──────────────────┐
        │                                 │
        ▼                                 ▼
┌──────────────────────────┐    ┌──────────────────────────┐
│ MockAD8232Sensor         │    │ CustomSensor             │
├──────────────────────────┤    ├──────────────────────────┤
│ - sampleCount : ulong    │    │ - state : ...            │
│ - samplingRate : int     │    │ - calibration : ...      │
├──────────────────────────┤    ├──────────────────────────┤
│ - getGaussianNoise()     │    │ + readSample()           │
│ + readSample() : float   │    │ + getSamplingRate()      │
│ + getSamplingRate() : int│    │ + getSensorId()          │
│ + getSensorId() : String │    │ + customMethod()         │
└──────────────────────────┘    └──────────────────────────┘


┌─────────────────────────────────────────────────────────────────┐
│ BiomedicalPipeline (Base Class)                                 │
├─────────────────────────────────────────────────────────────────┤
│ # sensor : IBiomedicalSensor*                                   │
│ # buffer : SignalBuffer<float>                                  │
│ # extractor : WindowExtractor                                   │
├─────────────────────────────────────────────────────────────────┤
│ + BiomedicalPipeline(sensor, windowSize, stepSize)              │
│ + tick() : void                                                 │
│ # onWindowReady(window*, size) : void [VIRTUAL]                │
│ + getSensor() : IBiomedicalSensor*                              │
│ + getBufferSize() : int                                         │
│ + reset() : void                                                │
└──────────────────────┬──────────────────────────────────────────┘
                       │ inherits + overrides onWindowReady()
                       │
                       ▼
               ┌────────────────────────┐
               │ CardioStreamApp        │
               ├────────────────────────┤
               │ - windowCount : ulong  │
               ├────────────────────────┤
               │ - computeMean()        │
               │ - computeVariance()    │
               │ + onWindowReady()      │
               │ + getWindowCount()     │
               └────────────────────────┘


Composition: BiomedicalPipeline contains SignalBuffer + WindowExtractor

┌─────────────────────────────────────────────────────────────────┐
│ SignalBuffer<T> (Template Ring Buffer)                          │
├─────────────────────────────────────────────────────────────────┤
│ - buffer : T*                                                   │
│ - head : int                                                    │
│ - count : int                                                   │
│ - maxSize : int                                                 │
├─────────────────────────────────────────────────────────────────┤
│ + push(value : T) : void                                        │
│ + getWindow(out*, size) : bool                                  │
│ + isFull() : bool                                               │
│ + size() : int                                                  │
│ + reset() : void                                                │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ WindowExtractor                                                 │
├─────────────────────────────────────────────────────────────────┤
│ - windowSize : int                                              │
│ - stepSize : int                                                │
│ - sampleCounter : int                                           │
├─────────────────────────────────────────────────────────────────┤
│ + extract(buf&, window*) : bool                                 │
│ + getWindowSize() : int                                         │
│ + getStepSize() : int                                           │
│ + reset() : void                                                │
└─────────────────────────────────────────────────────────────────┘


Data Flow:
┌───────────────────────────────────────────────────────────────────┐
│ Sensor                                                            │
│ readSample()                                                      │
│ ↓                                                                 │
│ ┌─ buffer.push(sample) ─────┐                                    │
│ │ [Ring Buffer]             │                                    │
│ │ [samples...← new sample]  │                                    │
│ └──────────────────┬────────┘                                    │
│                    ↓                                              │
│ extractor.extract(buffer, window)                                │
│ Returns: bool                                                    │
│ If stepSize samples collected && buffer ≥ windowSize:            │
│   ✓ Copy last windowSize samples to 'window'                     │
│   ✓ Return true                                                  │
│ Else:                                                            │
│   ✗ Return false                                                 │
│                    ↓                                              │
│ onWindowReady(window, size)  [Callback]                          │
│ User processes: mean, variance, cloud send, ML inference, etc.   │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

---

## Usage Example

### 1. Create a Custom Sensor

Implement `IBiomedicalSensor`:

```cpp
class MyAD8232 : public IBiomedicalSensor {
private:
    int adcPin;
    int samplingRate;
    
public:
    MyAD8232(int pin, int rate) 
        : adcPin(pin), samplingRate(rate) {}
    
    float readSample() override {
        return analogRead(adcPin) * 3.3f / 4095.0f;  // Convert to mV
    }
    
    int getSamplingRate() override {
        return samplingRate;  // e.g., 250 Hz
    }
    
    String getSensorId() override {
        return "AD8232_Channel_1";
    }
};
```

### 2. Create a Processing App

Inherit from `BiomedicalPipeline`:

```cpp
class HeartRateMonitor : public BiomedicalPipeline {
public:
    HeartRateMonitor(IBiomedicalSensor* sensor) 
        : BiomedicalPipeline(sensor, 256) {}  // 256 samples = 1 sec @ 250Hz
    
    void onWindowReady(float* window, int size) override {
        // Implement your signal processing here
        float mean = computeMean(window, size);
        float rms = computeRMS(window, size);
        
        // Example: detect R-peaks, estimate heart rate
        int bpm = estimateHeartRate(window, size);
        
        // Send to cloud / update display / trigger alert
        sendToCloud(bpm);
    }
    
private:
    float computeMean(float* data, int size) {
        float sum = 0.0f;
        for (int i = 0; i < size; i++) sum += data[i];
        return sum / size;
    }
    
    float computeRMS(float* data, int size) {
        float sumSq = 0.0f;
        for (int i = 0; i < size; i++) sumSq += data[i] * data[i];
        return sqrtf(sumSq / size);
    }
    
    int estimateHeartRate(float* window, int size) {
        // Your heart rate algorithm here
        // e.g., FFT-based peak detection, wavelet transform, etc.
        return 72;  // placeholder
    }
    
    void sendToCloud(int bpm) {
        // WiFi/MQTT publish
    }
};
```

### 3. Setup & Loop

```cpp
HeartRateMonitor* monitor = nullptr;

void setup() {
    Serial.begin(115200);
    
    // Create sensor
    IBiomedicalSensor* sensor = new MyAD8232(A0, 250);
    
    // Create app
    monitor = new HeartRateMonitor(sensor);
    
    Serial.print("Started: ");
    Serial.println(monitor->getSensor()->getSensorId());
}

void loop() {
    // Call tick() repeatedly; onWindowReady() fires automatically
    monitor->tick();
    
    // For 250 Hz sampling: tick every 4ms
    delayMicroseconds(4000);
}
```

---

## Configuration & Parameters

### Window Size

- **Default**: 256 samples
- **At 250 Hz ECG**: 256 / 250 = 1.024 seconds per window
- **At 500 Hz EMG**: 256 / 500 = 0.512 seconds per window
- **Trade-off**: Larger windows = more context, higher latency; smaller = faster response, less data

```cpp
BiomedicalPipeline* pipeline = new MyApp(sensor, 512);  // 512-sample windows
```

### Step Size (Window Overlap)

- **Default**: equals windowSize (non-overlapping)
- **Example**: `WindowExtractor(256, 128)` → 50% overlap

```cpp
BiomedicalPipeline* pipeline = new MyApp(sensor, 256, 128);  // 50% overlap
```

### Sampling Rate

Set in your `IBiomedicalSensor` implementation. Common rates:

- **250 Hz** — Standard clinical ECG
- **500 Hz** — Higher-precision EMG
- **100 Hz** — Simple heart rate (PPG)

---

## Performance Characteristics

| Operation | Time Complexity | Space | Notes |
|-----------|-----------------|-------|-------|
| `tick()` | O(1) | O(windowSize) | Ring buffer + window extraction |
| `push()` | O(1) | — | Single assignment + modulo |
| `getWindow()` | O(windowSize) | — | Copies windowSize floats |
| `extract()` | O(1) | — | Counter check + getWindow call |
| **Memory Overhead** | — | ~2KB (256-sample float buffer) | Minimal for embedded systems |

**Real-time Safety:**
- 250 Hz ECG @ 256-sample window: window fires every 1.024 seconds
- `tick()` must complete in < 4ms (for 250 Hz sample rate)
- No dynamic allocation during `tick()` → predictable latency

---

## File Structure

```
CardioStream-IoT-Framework/
├── src/
│   ├── IBiomedicalSensor.h       # Abstract sensor interface
│   ├── SignalBuffer.h             # Circular ring buffer template
│   ├── WindowExtractor.h          # Window extraction logic
│   └── BiomedicalPipeline.h       # Orchestrator base class
├── examples/
│   └── ecg_brazalete/
│       └── main.cpp               # Example: MockAD8232 + CardioStreamApp
├── CMakeLists.txt                 # Build configuration
└── README.md                      # This file
```

---

## Building & Running

### For ESP32 (Arduino IDE)

1. Copy `src/` to your Arduino libraries folder
2. Include in your sketch: `#include "BiomedicalPipeline.h"`
3. Implement your sensor and app classes
4. Build & upload to ESP32

### For Desktop / Testing (CMake)

```bash
cd CardioStream-IoT-Framework
mkdir build && cd build
cmake ..
make

# Run example
./cardiostream_ecg_demo
```

**Note**: Desktop build requires mocking `Arduino.h` header.

---

## Future Extensions

- **Signal Processing Library**: FFT, wavelet transforms, filtering
- **Machine Learning**: Real-time inference for arrhythmia detection
- **Cloud Integration**: AWS IoT, Azure IoT Hub adapters
- **Multi-Sensor Support**: Fuse ECG + PPG + IMU data
- **Logging Framework**: SD card or cloud storage for signal recording
- **Visualization**: Web dashboard for real-time monitoring

---

## License

MIT License — Feel free to use, modify, and distribute.

---

## References

- **ESP32 Developer Guide**: https://docs.espressif.com/projects/esp-idf/
- **AD8232 ECG Sensor**: https://learn.sparkfun.com/tutorials/ad8232-heart-rate-monitor-sensor
- **Real-time Systems Design**: Liu & Layland (1973), Rate-Monotonic Scheduling
- **Signal Processing on Microcontrollers**: O'Flynn & Ryan (2015)

---

**Questions or Issues?**

Open an issue or email: [your contact]

Happy biomedical signal processing! 🫀

