#include "serializer.hpp"
#include "alarm_mqtt_msg.hpp"
#include <iostream>
#include <chrono>
#include <cstring>
#include <cmath>
#include <iomanip>

using namespace waterworld::alarm;

// ANSI color codes
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_RESET "\033[0m"

// Test result tracking
int tests_passed = 0;
int tests_failed = 0;

// Helper function to print test result
void printTestResult(const std::string& test_name, bool passed) {
    if (passed) {
        std::cout << COLOR_GREEN << "[PASS]" << COLOR_RESET << " " << test_name << std::endl;
        tests_passed++;
    } else {
        std::cout << COLOR_RED << "[FAIL]" << COLOR_RESET << " " << test_name << std::endl;
        tests_failed++;
    }
}

// Test Header serialization/deserialization
bool testHeader() {
    Header original;
    original.frame_id = "test_frame";
    original.device_id = "device_001";
    original.timestamp = 1729358400123456789LL;
    
    // Serialize
    std::vector<uint8_t> buffer;
    Serializer::serializeHeader(buffer, original);
    
    // Deserialize
    size_t offset = 0;
    Header deserialized = Serializer::deserializeHeader(buffer.data(), offset, buffer.size());
    
    // Verify
    bool passed = (original.frame_id == deserialized.frame_id) &&
                  (original.device_id == deserialized.device_id) &&
                  (original.timestamp == deserialized.timestamp);
    
    return passed;
}

// Test VitalData serialization/deserialization
bool testVitalData() {
    VitalData original;
    original.header.frame_id = "vital_frame";
    original.header.device_id = "device_002";
    original.header.timestamp = 1729358400000000000LL;
    original.init_stat = 1;
    original.maxd = 100;
    original.conf = 0.95f;
    original.hr_est = 72.5f;
    original.rr_est = 18.3f;
    original.rr_amp = 0.8f;
    
    // Serialize
    std::vector<uint8_t> binary = Serializer::serializeVitalData(original);
    
    // Deserialize
    VitalData deserialized = Serializer::deserializeVitalData(binary.data(), binary.size());
    
    // Verify
    bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                  (original.header.device_id == deserialized.header.device_id) &&
                  (original.header.timestamp == deserialized.header.timestamp) &&
                  (original.init_stat == deserialized.init_stat) &&
                  (original.maxd == deserialized.maxd) &&
                  (std::abs(original.conf - deserialized.conf) < 1e-6f) &&
                  (std::abs(original.hr_est - deserialized.hr_est) < 1e-6f) &&
                  (std::abs(original.rr_est - deserialized.rr_est) < 1e-6f) &&
                  (std::abs(original.rr_amp - deserialized.rr_amp) < 1e-6f);
    
    return passed;
}

// Test PresenceData serialization/deserialization
bool testPresenceData() {
    PresenceData original;
    original.header.frame_id = "presence_frame";
    original.header.device_id = "device_003";
    original.header.timestamp = 1729358400111111111LL;
    original.presence_status = 1;
    
    // Serialize
    std::vector<uint8_t> binary = Serializer::serializePresenceData(original);
    
    // Deserialize
    PresenceData deserialized = Serializer::deserializePresenceData(binary.data(), binary.size());
    
    // Verify
    bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                  (original.header.device_id == deserialized.header.device_id) &&
                  (original.header.timestamp == deserialized.header.timestamp) &&
                  (original.presence_status == deserialized.presence_status);
    
    return passed;
}

// Test SleepStatus serialization/deserialization
bool testSleepStatus() {
    SleepStatus original;
    original.header.frame_id = "sleep_frame";
    original.header.device_id = "device_004";
    original.header.timestamp = 1729358400222222222LL;
    original.sleep_status = 2;
    original.avg_hr_rate = 65;
    original.avg_rr_rate = 16;
    
    // Serialize
    std::vector<uint8_t> binary = Serializer::serializeSleepStatus(original);
    
    // Deserialize
    SleepStatus deserialized = Serializer::deserializeSleepStatus(binary.data(), binary.size());
    
    // Verify
    bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                  (original.header.device_id == deserialized.header.device_id) &&
                  (original.header.timestamp == deserialized.header.timestamp) &&
                  (original.sleep_status == deserialized.sleep_status) &&
                  (original.avg_hr_rate == deserialized.avg_hr_rate) &&
                  (original.avg_rr_rate == deserialized.avg_rr_rate);
    
    return passed;
}

// Test MMWaveData serialization/deserialization
bool testMMWaveData() {
    MMWaveData original;
    original.header.frame_id = "mmwave_frame";
    original.header.device_id = "MMW_RADAR_001";
    original.header.timestamp = 1729358400123456789LL;
    
    // Fill with test pattern
    for (int i = 0; i < 2048; ++i) {
        original.adc_data[i] = static_cast<int16_t>(i - 1024); // Range: -1024 to 1023
    }
    
    // Serialize
    std::vector<uint8_t> binary = Serializer::serializeMMWaveData(original);
    
    // Deserialize
    MMWaveData deserialized = Serializer::deserializeMMWaveData(binary.data(), binary.size());
    
    // Verify header
    bool header_ok = (original.header.frame_id == deserialized.header.frame_id) &&
                     (original.header.device_id == deserialized.header.device_id) &&
                     (original.header.timestamp == deserialized.header.timestamp);
    
    // Verify all ADC samples
    bool adc_ok = true;
    for (int i = 0; i < 2048; ++i) {
        if (original.adc_data[i] != deserialized.adc_data[i]) {
            std::cerr << "  Mismatch at index " << i 
                      << ": original=" << original.adc_data[i]
                      << ", deserialized=" << deserialized.adc_data[i] << std::endl;
            adc_ok = false;
            break;
        }
    }
    
    return header_ok && adc_ok;
}

// Test SleepReportRequest serialization/deserialization
bool testSleepReportRequest() {
    SleepReportRequest original;
    original.header.frame_id = "sleep_request_frame";
    original.header.device_id = "device_005";
    original.header.timestamp = 1729358400333333333LL;
    original.request_id = "req_12345_abcde";
    
    // Serialize
    std::vector<uint8_t> binary = Serializer::serializeSleepReportRequest(original);
    
    // Deserialize
    SleepReportRequest deserialized = Serializer::deserializeSleepReportRequest(binary.data(), binary.size());
    
    // Verify
    bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                  (original.header.device_id == deserialized.header.device_id) &&
                  (original.header.timestamp == deserialized.header.timestamp) &&
                  (original.request_id == deserialized.request_id);
    
    return passed;
}

// Test SleepReportResponse serialization/deserialization
bool testSleepReportResponse() {
    SleepReportResponse original;
    original.header.frame_id = "sleep_response_frame";
    original.header.device_id = "device_006";
    original.header.timestamp = 1729358400444444444LL;
    original.request_id = "req_12345_abcde";
    original.sleep_start_time = 1729350000000000000LL;
    original.sleep_end_time = 1729378800000000000LL;
    original.sleep_duration = 28800; // 8 hours in seconds
    original.woke_up_count = 3;
    original.body_movement_count = 15;
    original.deep_sleep_percentage = 35;
    original.light_sleep_percentage = 65;
    original.avg_hr_rate = 62;
    original.avg_rr_rate = 15;
    original.sleep_status_graph_len = 96; // 8 hours * 12 (5-min intervals)
    original.sleep_status_graph_resolution = 300; // 5 minutes in seconds
    
    // Fill vectors with test data
    original.sleep_stages_graph = {1, 1, 2, 2, 3, 3, 2, 1, 0, 1, 2, 3};
    original.hr_graph_array = {65, 64, 62, 60, 58, 59, 61, 63, 64, 66, 67, 68};
    original.rr_graph_array = {16, 15, 14, 13, 14, 15, 15, 16, 16, 17, 17, 16};
    original.body_movement_graph = {0, 1, 0, 2, 0, 0, 1, 0, 3, 0, 1, 0};
    
    // Serialize
    std::vector<uint8_t> binary = Serializer::serializeSleepReportResponse(original);
    
    // Deserialize
    SleepReportResponse deserialized = Serializer::deserializeSleepReportResponse(binary.data(), binary.size());
    
    // Verify header and basic fields
    bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                  (original.header.device_id == deserialized.header.device_id) &&
                  (original.header.timestamp == deserialized.header.timestamp) &&
                  (original.request_id == deserialized.request_id) &&
                  (original.sleep_start_time == deserialized.sleep_start_time) &&
                  (original.sleep_end_time == deserialized.sleep_end_time) &&
                  (original.sleep_duration == deserialized.sleep_duration) &&
                  (original.woke_up_count == deserialized.woke_up_count) &&
                  (original.body_movement_count == deserialized.body_movement_count) &&
                  (original.deep_sleep_percentage == deserialized.deep_sleep_percentage) &&
                  (original.light_sleep_percentage == deserialized.light_sleep_percentage) &&
                  (original.avg_hr_rate == deserialized.avg_hr_rate) &&
                  (original.avg_rr_rate == deserialized.avg_rr_rate) &&
                  (original.sleep_status_graph_len == deserialized.sleep_status_graph_len) &&
                  (original.sleep_status_graph_resolution == deserialized.sleep_status_graph_resolution);
    
    // Verify vectors
    passed = passed && (original.sleep_stages_graph == deserialized.sleep_stages_graph);
    passed = passed && (original.hr_graph_array == deserialized.hr_graph_array);
    passed = passed && (original.rr_graph_array == deserialized.rr_graph_array);
    passed = passed && (original.body_movement_graph == deserialized.body_movement_graph);
    
    return passed;
}

// Test ErrorCode serialization/deserialization
bool testErrorCode() {
    ErrorCode original;
    original.header.frame_id = "error_frame";
    original.header.device_id = "device_007";
    original.header.timestamp = 1729358400555555555LL;
    original.code = "ERR_SENSOR_FAULT";
    original.message = "Radar sensor communication timeout after 5 retries";
    
    // Serialize
    std::vector<uint8_t> binary = Serializer::serializeErrorCode(original);
    
    // Deserialize
    ErrorCode deserialized = Serializer::deserializeErrorCode(binary.data(), binary.size());
    
    // Verify
    bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                  (original.header.device_id == deserialized.header.device_id) &&
                  (original.header.timestamp == deserialized.header.timestamp) &&
                  (original.code == deserialized.code) &&
                  (original.message == deserialized.message);
    
    return passed;
}

// Performance test for MMWaveData
void performanceTestMMWaveData() {
    std::cout << "\n" << COLOR_BLUE << "=== Performance Test: MMWaveData ===" << COLOR_RESET << std::endl;
    
    MMWaveData test_data;
    test_data.header.frame_id = "perf_test";
    test_data.header.device_id = "PERF_DEVICE";
    test_data.header.timestamp = 1729358400000000000LL;
    
    // Fill with random-like data
    for (int i = 0; i < 2048; ++i) {
        test_data.adc_data[i] = static_cast<int16_t>((i * 13) % 4096 - 2048);
    }
    
    const int iterations = 10000;
    
    // Serialization performance
    auto start_serialize = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::vector<uint8_t> binary = Serializer::serializeMMWaveData(test_data);
        // Prevent optimization from removing the call
        volatile size_t size = binary.size();
        (void)size;
    }
    auto end_serialize = std::chrono::high_resolution_clock::now();
    auto serialize_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_serialize - start_serialize
    );
    
    // Deserialization performance
    std::vector<uint8_t> serialized = Serializer::serializeMMWaveData(test_data);
    auto start_deserialize = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        MMWaveData result = Serializer::deserializeMMWaveData(serialized.data(), serialized.size());
        // Prevent optimization from removing the call
        volatile int16_t val = result.adc_data[0];
        (void)val;
    }
    auto end_deserialize = std::chrono::high_resolution_clock::now();
    auto deserialize_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_deserialize - start_deserialize
    );
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Serialization:" << std::endl;
    std::cout << "  Total time: " << serialize_duration.count() << " μs" << std::endl;
    std::cout << "  Average: " << (serialize_duration.count() / static_cast<double>(iterations)) << " μs/op" << std::endl;
    std::cout << "  Payload size: " << serialized.size() << " bytes" << std::endl;
    
    std::cout << "Deserialization:" << std::endl;
    std::cout << "  Total time: " << deserialize_duration.count() << " μs" << std::endl;
    std::cout << "  Average: " << (deserialize_duration.count() / static_cast<double>(iterations)) << " μs/op" << std::endl;
    
    std::cout << "Round-trip average: " 
              << ((serialize_duration.count() + deserialize_duration.count()) / static_cast<double>(iterations)) 
              << " μs/op" << std::endl;
}

// Test edge cases for MMWaveData
bool testMMWaveDataEdgeCases() {
    std::cout << "\n" << COLOR_YELLOW << "Testing MMWaveData edge cases..." << COLOR_RESET << std::endl;
    
    bool all_passed = true;
    
    // Test 1: All zeros
    {
        MMWaveData original;
        original.header.frame_id = "edge_zeros";
        original.header.device_id = "EDGE_DEV";
        original.header.timestamp = 1729358400000000000LL;
        std::memset(original.adc_data, 0, sizeof(original.adc_data));
        
        std::vector<uint8_t> binary = Serializer::serializeMMWaveData(original);
        MMWaveData deserialized = Serializer::deserializeMMWaveData(binary.data(), binary.size());
        
        bool passed = std::memcmp(original.adc_data, deserialized.adc_data, sizeof(original.adc_data)) == 0;
        printTestResult("  Edge case: All zeros", passed);
        all_passed &= passed;
    }
    
    // Test 2: Max positive values
    {
        MMWaveData original;
        original.header.frame_id = "edge_max";
        original.header.device_id = "EDGE_DEV";
        original.header.timestamp = 1729358400000000000LL;
        for (int i = 0; i < 2048; ++i) {
            original.adc_data[i] = 32767; // INT16_MAX
        }
        
        std::vector<uint8_t> binary = Serializer::serializeMMWaveData(original);
        MMWaveData deserialized = Serializer::deserializeMMWaveData(binary.data(), binary.size());
        
        bool passed = std::memcmp(original.adc_data, deserialized.adc_data, sizeof(original.adc_data)) == 0;
        printTestResult("  Edge case: Max positive (32767)", passed);
        all_passed &= passed;
    }
    
    // Test 3: Max negative values
    {
        MMWaveData original;
        original.header.frame_id = "edge_min";
        original.header.device_id = "EDGE_DEV";
        original.header.timestamp = 1729358400000000000LL;
        for (int i = 0; i < 2048; ++i) {
            original.adc_data[i] = -32768; // INT16_MIN
        }
        
        std::vector<uint8_t> binary = Serializer::serializeMMWaveData(original);
        MMWaveData deserialized = Serializer::deserializeMMWaveData(binary.data(), binary.size());
        
        bool passed = std::memcmp(original.adc_data, deserialized.adc_data, sizeof(original.adc_data)) == 0;
        printTestResult("  Edge case: Max negative (-32768)", passed);
        all_passed &= passed;
    }
    
    // Test 4: Alternating pattern
    {
        MMWaveData original;
        original.header.frame_id = "edge_alt";
        original.header.device_id = "EDGE_DEV";
        original.header.timestamp = 1729358400000000000LL;
        for (int i = 0; i < 2048; ++i) {
            original.adc_data[i] = (i % 2 == 0) ? 32767 : -32768;
        }
        
        std::vector<uint8_t> binary = Serializer::serializeMMWaveData(original);
        MMWaveData deserialized = Serializer::deserializeMMWaveData(binary.data(), binary.size());
        
        bool passed = std::memcmp(original.adc_data, deserialized.adc_data, sizeof(original.adc_data)) == 0;
        printTestResult("  Edge case: Alternating max/min", passed);
        all_passed &= passed;
    }
    
    return all_passed;
}

// Test edge cases for string serialization
bool testStringEdgeCases() {
    std::cout << "\n" << COLOR_YELLOW << "Testing String edge cases..." << COLOR_RESET << std::endl;
    
    bool all_passed = true;
    
    // Test 1: Empty strings
    {
        Header original;
        original.frame_id = "";
        original.device_id = "";
        original.timestamp = 1729358400000000000LL;
        
        std::vector<uint8_t> buffer;
        Serializer::serializeHeader(buffer, original);
        
        size_t offset = 0;
        Header deserialized = Serializer::deserializeHeader(buffer.data(), offset, buffer.size());
        
        bool passed = (original.frame_id == deserialized.frame_id) &&
                      (original.device_id == deserialized.device_id) &&
                      (original.timestamp == deserialized.timestamp);
        printTestResult("  Edge case: Empty strings", passed);
        all_passed &= passed;
    }
    
    // Test 2: Very long strings
    {
        Header original;
        original.frame_id = std::string(1000, 'A');
        original.device_id = std::string(2000, 'B');
        original.timestamp = 1729358400000000000LL;
        
        std::vector<uint8_t> buffer;
        Serializer::serializeHeader(buffer, original);
        
        size_t offset = 0;
        Header deserialized = Serializer::deserializeHeader(buffer.data(), offset, buffer.size());
        
        bool passed = (original.frame_id == deserialized.frame_id) &&
                      (original.device_id == deserialized.device_id) &&
                      (original.timestamp == deserialized.timestamp);
        printTestResult("  Edge case: Very long strings (1000, 2000 chars)", passed);
        all_passed &= passed;
    }
    
    // Test 3: Special characters
    {
        Header original;
        original.frame_id = "test\n\r\t\0special";
        original.device_id = "UTF-8: 你好世界 🌍";
        original.timestamp = 1729358400000000000LL;
        
        std::vector<uint8_t> buffer;
        Serializer::serializeHeader(buffer, original);
        
        size_t offset = 0;
        Header deserialized = Serializer::deserializeHeader(buffer.data(), offset, buffer.size());
        
        bool passed = (original.frame_id == deserialized.frame_id) &&
                      (original.device_id == deserialized.device_id) &&
                      (original.timestamp == deserialized.timestamp);
        printTestResult("  Edge case: Special characters and UTF-8", passed);
        all_passed &= passed;
    }
    
    return all_passed;
}

// Test edge cases for numeric types
bool testNumericEdgeCases() {
    std::cout << "\n" << COLOR_YELLOW << "Testing Numeric edge cases..." << COLOR_RESET << std::endl;
    
    bool all_passed = true;
    
    // Test 1: INT64_MIN and INT64_MAX timestamps
    {
        Header original;
        original.frame_id = "int64_test";
        original.device_id = "device";
        original.timestamp = INT64_MIN;
        
        std::vector<uint8_t> buffer;
        Serializer::serializeHeader(buffer, original);
        
        size_t offset = 0;
        Header deserialized = Serializer::deserializeHeader(buffer.data(), offset, buffer.size());
        
        bool passed_min = (original.timestamp == deserialized.timestamp);
        printTestResult("  Edge case: INT64_MIN timestamp", passed_min);
        all_passed &= passed_min;
        
        // Test INT64_MAX
        original.timestamp = INT64_MAX;
        buffer.clear();
        Serializer::serializeHeader(buffer, original);
        offset = 0;
        deserialized = Serializer::deserializeHeader(buffer.data(), offset, buffer.size());
        
        bool passed_max = (original.timestamp == deserialized.timestamp);
        printTestResult("  Edge case: INT64_MAX timestamp", passed_max);
        all_passed &= passed_max;
    }
    
    // Test 2: INT32 limits in VitalData
    {
        VitalData original;
        original.header.frame_id = "int32_test";
        original.header.device_id = "device";
        original.header.timestamp = 0;
        original.init_stat = INT32_MIN;
        original.maxd = INT32_MAX;
        original.conf = 0.0f;
        original.hr_est = 0.0f;
        original.rr_est = 0.0f;
        original.rr_amp = 0.0f;
        
        std::vector<uint8_t> binary = Serializer::serializeVitalData(original);
        VitalData deserialized = Serializer::deserializeVitalData(binary.data(), binary.size());
        
        bool passed = (original.init_stat == deserialized.init_stat) &&
                      (original.maxd == deserialized.maxd);
        printTestResult("  Edge case: INT32_MIN and INT32_MAX", passed);
        all_passed &= passed;
    }
    
    // Test 3: Negative int32 values
    {
        VitalData original;
        original.header.frame_id = "negative_test";
        original.header.device_id = "device";
        original.header.timestamp = 0;
        original.init_stat = -12345;
        original.maxd = -67890;
        original.conf = 0.0f;
        original.hr_est = 0.0f;
        original.rr_est = 0.0f;
        original.rr_amp = 0.0f;
        
        std::vector<uint8_t> binary = Serializer::serializeVitalData(original);
        VitalData deserialized = Serializer::deserializeVitalData(binary.data(), binary.size());
        
        bool passed = (original.init_stat == deserialized.init_stat) &&
                      (original.maxd == deserialized.maxd);
        printTestResult("  Edge case: Negative int32 values", passed);
        all_passed &= passed;
    }
    
    // Test 4: Float special values
    {
        VitalData original;
        original.header.frame_id = "float_test";
        original.header.device_id = "device";
        original.header.timestamp = 0;
        original.init_stat = 0;
        original.maxd = 0;
        original.conf = 0.0f;
        original.hr_est = -0.0f;
        original.rr_est = 1.234567890123456f;  // Test precision
        original.rr_amp = -9876.54321f;
        
        std::vector<uint8_t> binary = Serializer::serializeVitalData(original);
        VitalData deserialized = Serializer::deserializeVitalData(binary.data(), binary.size());
        
        bool passed = (original.conf == deserialized.conf) &&
                      (original.hr_est == deserialized.hr_est) &&
                      (std::abs(original.rr_est - deserialized.rr_est) < 1e-6f) &&
                      (std::abs(original.rr_amp - deserialized.rr_amp) < 1e-3f);
        printTestResult("  Edge case: Float special values", passed);
        all_passed &= passed;
    }
    
    return all_passed;
}

// Test all field completeness
bool testFieldCompleteness() {
    std::cout << "\n" << COLOR_YELLOW << "Testing field completeness..." << COLOR_RESET << std::endl;
    
    bool all_passed = true;
    
    // Verify VitalData has all fields
    {
        VitalData original;
        original.header.frame_id = "complete_test";
        original.header.device_id = "device_complete";
        original.header.timestamp = 1234567890123456789LL;
        original.init_stat = 100;
        original.maxd = 200;
        original.conf = 0.85f;
        original.hr_est = 75.5f;
        original.rr_est = 16.8f;
        original.rr_amp = 0.95f;
        
        std::vector<uint8_t> binary = Serializer::serializeVitalData(original);
        VitalData deserialized = Serializer::deserializeVitalData(binary.data(), binary.size());
        
        bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                      (original.header.device_id == deserialized.header.device_id) &&
                      (original.header.timestamp == deserialized.header.timestamp) &&
                      (original.init_stat == deserialized.init_stat) &&
                      (original.maxd == deserialized.maxd) &&
                      (std::abs(original.conf - deserialized.conf) < 1e-6f) &&
                      (std::abs(original.hr_est - deserialized.hr_est) < 1e-6f) &&
                      (std::abs(original.rr_est - deserialized.rr_est) < 1e-6f) &&
                      (std::abs(original.rr_amp - deserialized.rr_amp) < 1e-6f);
        
        printTestResult("  VitalData: All 9 fields present", passed);
        all_passed &= passed;
    }
    
    // Verify PresenceData has all fields
    {
        PresenceData original;
        original.header.frame_id = "presence_complete";
        original.header.device_id = "device_presence";
        original.header.timestamp = 9876543210987654321LL;
        original.presence_status = 1;
        
        std::vector<uint8_t> binary = Serializer::serializePresenceData(original);
        PresenceData deserialized = Serializer::deserializePresenceData(binary.data(), binary.size());
        
        bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                      (original.header.device_id == deserialized.header.device_id) &&
                      (original.header.timestamp == deserialized.header.timestamp) &&
                      (original.presence_status == deserialized.presence_status);
        
        printTestResult("  PresenceData: All 4 fields present", passed);
        all_passed &= passed;
    }
    
    // Verify SleepStatus has all fields
    {
        SleepStatus original;
        original.header.frame_id = "sleep_complete";
        original.header.device_id = "device_sleep";
        original.header.timestamp = 5555555555555555555LL;
        original.sleep_status = 3;
        original.avg_hr_rate = 68;
        original.avg_rr_rate = 14;
        
        std::vector<uint8_t> binary = Serializer::serializeSleepStatus(original);
        SleepStatus deserialized = Serializer::deserializeSleepStatus(binary.data(), binary.size());
        
        bool passed = (original.header.frame_id == deserialized.header.frame_id) &&
                      (original.header.device_id == deserialized.header.device_id) &&
                      (original.header.timestamp == deserialized.header.timestamp) &&
                      (original.sleep_status == deserialized.sleep_status) &&
                      (original.avg_hr_rate == deserialized.avg_hr_rate) &&
                      (original.avg_rr_rate == deserialized.avg_rr_rate);
        
        printTestResult("  SleepStatus: All 6 fields present", passed);
        all_passed &= passed;
    }
    
    // Verify MMWaveData has all fields
    {
        MMWaveData original;
        original.header.frame_id = "mmwave_complete";
        original.header.device_id = "device_mmwave";
        original.header.timestamp = 1111111111111111111LL;
        for (int i = 0; i < 2048; ++i) {
            original.adc_data[i] = static_cast<int16_t>(i * 7 % 32767);
        }
        
        std::vector<uint8_t> binary = Serializer::serializeMMWaveData(original);
        MMWaveData deserialized = Serializer::deserializeMMWaveData(binary.data(), binary.size());
        
        bool header_ok = (original.header.frame_id == deserialized.header.frame_id) &&
                         (original.header.device_id == deserialized.header.device_id) &&
                         (original.header.timestamp == deserialized.header.timestamp);
        
        bool adc_ok = (std::memcmp(original.adc_data, deserialized.adc_data, sizeof(original.adc_data)) == 0);
        
        bool passed = header_ok && adc_ok;
        printTestResult("  MMWaveData: All fields (header + 2048 samples) present", passed);
        all_passed &= passed;
    }
    
    return all_passed;
}

int main() {
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "  Serializer Test Suite" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << std::endl;
    
    // Run basic tests
    std::cout << COLOR_YELLOW << "Running basic serialization tests..." << COLOR_RESET << std::endl;
    printTestResult("Header serialization", testHeader());
    printTestResult("VitalData serialization", testVitalData());
    printTestResult("PresenceData serialization", testPresenceData());
    printTestResult("SleepStatus serialization", testSleepStatus());
    printTestResult("SleepReportRequest serialization", testSleepReportRequest());
    printTestResult("SleepReportResponse serialization", testSleepReportResponse());
    printTestResult("ErrorCode serialization", testErrorCode());
    printTestResult("MMWaveData serialization", testMMWaveData());
    
    // Run field completeness tests
    bool completeness_passed = testFieldCompleteness();
    
    // Run edge case tests
    bool string_edge_passed = testStringEdgeCases();
    bool numeric_edge_passed = testNumericEdgeCases();
    bool mmwave_edge_passed = testMMWaveDataEdgeCases();
    
    // Run performance tests
    performanceTestMMWaveData();
    
    // Print summary
    std::cout << "\n" << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "  Test Summary" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "========================================" << COLOR_RESET << std::endl;
    std::cout << "Tests passed: " << COLOR_GREEN << tests_passed << COLOR_RESET << std::endl;
    std::cout << "Tests failed: " << (tests_failed > 0 ? COLOR_RED : COLOR_GREEN) 
              << tests_failed << COLOR_RESET << std::endl;
    std::cout << "Total tests: " << (tests_passed + tests_failed) << std::endl;
    
    if (tests_failed == 0) {
        std::cout << "\n" << COLOR_GREEN << "✓ All tests passed!" << COLOR_RESET << std::endl;
        return 0;
    } else {
        std::cout << "\n" << COLOR_RED << "✗ Some tests failed!" << COLOR_RESET << std::endl;
        return 1;
    }
}
