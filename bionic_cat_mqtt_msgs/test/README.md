# Testing alarm_mqtt_msgs Serializer

## Overview

This directory contains tests for the serializer/deserializer functionality of the alarm_mqtt_msgs module.

## Building Tests

### Enable Tests During CMake Configuration

```bash
cd /home/finnox/wtwd_ws/waterworld_alarm_framework/build
cmake -DBUILD_TESTS=ON ..
make test_serializer
```

Or rebuild everything with tests:

```bash
cd /home/finnox/wtwd_ws/waterworld_alarm_framework
rm -rf build
mkdir build && cd build
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
```

## Running Tests

After building with `BUILD_TESTS=ON`:

```bash
# From build directory
./bin/test_serializer

# Or from install directory
cd /home/finnox/wtwd_ws/waterworld_alarm_framework/install/bin
./test_serializer
```

## Test Coverage

The test suite covers:

### 1. Basic Serialization Tests
- **Header**: Tests frame_id, device_id, and timestamp serialization
- **VitalData**: Tests all vital signs data fields
- **PresenceData**: Tests presence detection data
- **SleepStatus**: Tests sleep monitoring data
- **MMWaveData**: Tests 2048 ADC samples serialization

### 2. Edge Case Tests for MMWaveData
- All zeros
- Maximum positive values (32767)
- Maximum negative values (-32768)
- Alternating max/min pattern

### 3. Performance Tests
- Serialization speed (10,000 iterations)
- Deserialization speed (10,000 iterations)
- Round-trip time measurement
- Reports average time in microseconds per operation

## Expected Output

```
========================================
  Serializer Test Suite
========================================

Running basic serialization tests...
[PASS] Header serialization
[PASS] VitalData serialization
[PASS] PresenceData serialization
[PASS] SleepStatus serialization
[PASS] MMWaveData serialization

Testing MMWaveData edge cases...
  [PASS] Edge case: All zeros
  [PASS] Edge case: Max positive (32767)
  [PASS] Edge case: Max negative (-32768)
  [PASS] Edge case: Alternating max/min

=== Performance Test: MMWaveData ===
Iterations: 10000
Serialization:
  Total time: XXXXX μs
  Average: XX.XX μs/op
  Payload size: 4234 bytes
Deserialization:
  Total time: XXXXX μs
  Average: XX.XX μs/op
Round-trip average: XX.XX μs/op

========================================
  Test Summary
========================================
Tests passed: 9
Tests failed: 0
Total tests: 9

✓ All tests passed!
```

## Performance Expectations

Based on the optimizations:

- **Serialization**: < 100 μs per operation
- **Deserialization**: < 100 μs per operation
- **Round-trip**: < 200 μs per operation

With the optimized implementation using:
- Buffer pre-allocation
- Loop unrolling (4x)
- Direct pointer arithmetic
- Compile-time endianness detection

Expected improvements over naive implementation: **3-5x faster**

## Troubleshooting

### Test Fails to Build

Make sure you enabled tests:
```bash
cmake -DBUILD_TESTS=ON ..
```

### Performance is Slower Than Expected

1. Check compiler optimization level:
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Release ..
   ```

2. Ensure you're not running in debug mode

3. Check system load during testing

## Adding New Tests

To add new test cases, edit `test_serializer.cpp`:

1. Create a test function that returns `bool`
2. Add test logic with assertions
3. Call `printTestResult()` with the test name and result
4. Add the test call in `main()`

Example:
```cpp
bool testNewFeature() {
    // Your test logic here
    bool passed = (expected == actual);
    return passed;
}

// In main():
printTestResult("New feature test", testNewFeature());
```
