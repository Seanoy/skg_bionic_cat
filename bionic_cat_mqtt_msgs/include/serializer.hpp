#ifndef BIONIC_CAT_MSGS_SERIALIZER_HPP
#define BIONIC_CAT_MSGS_SERIALIZER_HPP

#include "bionic_cat_mqtt_msg.hpp"
#include <iostream>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>
#include <cstdint>

namespace BionicCat {
namespace MsgsSerializer {

using ::BionicCat::MqttMsgs::Header;
using ::BionicCat::MqttMsgs::HeartbeatMsg;
using ::BionicCat::MqttMsgs::MotorControlMsg;
using ::BionicCat::MqttMsgs::TempControlMsg;
using ::BionicCat::MqttMsgs::PowerControlMsg;
using ::BionicCat::MqttMsgs::RawTouchStatusMsg;
using ::BionicCat::MqttMsgs::RawTouchEventMsg;
using ::BionicCat::MqttMsgs::TouchPanelInteractionStatus;
using ::BionicCat::MqttMsgs::TouchStatusMsg;
using ::BionicCat::MqttMsgs::MotorStatusMsg;
using ::BionicCat::MqttMsgs::TempStatusMsg;
using ::BionicCat::MqttMsgs::ImuStatusMsg;
using ::BionicCat::MqttMsgs::SysStatusMsg;
using ::BionicCat::MqttMsgs::APwrRequest;
using ::BionicCat::MqttMsgs::BImuState;
using ::BionicCat::MqttMsgs::BTempErrorState;
using ::BionicCat::MqttMsgs::BChargeState;
using ::BionicCat::MqttMsgs::VitalData;
using ::BionicCat::MqttMsgs::VitalVisData;
using ::BionicCat::MqttMsgs::LLMEmotionDetResult;
using ::BionicCat::MqttMsgs::LLMEmotionIntention;
using ::BionicCat::MqttMsgs::VisualFeatureFrame;
using ::BionicCat::MqttMsgs::EyeballDispalyCommand;
using ::BionicCat::MqttMsgs::AudioPlayCommand;
using ::BionicCat::MqttMsgs::LedControlMsg;
using ::BionicCat::MqttMsgs::MotorControllerType;
using ::BionicCat::MqttMsgs::MotorControllerConfig;
using ::BionicCat::MqttMsgs::ActionGroupExecuteCommand;
using ::BionicCat::MqttMsgs::AdtsStreamControlMsg;
using ::BionicCat::MqttMsgs::AdtsStreamDataMsg;
using ::BionicCat::MqttMsgs::SystemStatInfo;
using ::BionicCat::MqttMsgs::ButtonStatusEventMsg;
using ::BionicCat::MqttMsgs::SoundLocalizationMsg; // 新增

/**
 * @brief Binary serializer/deserializer utilities (big-endian)
 *
 * Conventions:
 * - Multi-byte integers are encoded in big-endian (network byte order).
 * - Strings are encoded as [4-byte length (big-endian)] + [raw bytes].
 * - Floats are encoded by copying IEEE754 bits and writing as a 32-bit big-endian integer.
 * - All decode functions throw std::runtime_error when data is insufficient.
 */
class Serializer {
public:
    // --------- Primitive helpers ---------
    /**
     * @brief Write a length-prefixed string
     * @param buffer Destination buffer to append into
     * @param str Source string
     */
    static void serializeString(std::vector<uint8_t>& buffer, const std::string& str) {
        uint32_t length = static_cast<uint32_t>(str.length());
        buffer.push_back((length >> 24) & 0xFF);
        buffer.push_back((length >> 16) & 0xFF);
        buffer.push_back((length >> 8) & 0xFF);
        buffer.push_back(length & 0xFF);
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    /**
     * @brief Read a length-prefixed string
     * @param data Source buffer pointer
     * @param offset Read offset (will be advanced)
     * @param size Total size of source buffer
     * @return Decoded string
     */
    static std::string deserializeString(const uint8_t* data, size_t& offset, size_t size) {
        if (offset + 4 > size) throw std::runtime_error("Insufficient data to read string length");
        uint32_t length = (static_cast<uint32_t>(data[offset]) << 24) |
                          (static_cast<uint32_t>(data[offset + 1]) << 16) |
                          (static_cast<uint32_t>(data[offset + 2]) << 8) |
                          static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
        if (offset + length > size) throw std::runtime_error("Insufficient data to read string content");
        std::string result(reinterpret_cast<const char*>(data + offset), length);
        offset += length;
        return result;
    }

    /** @brief Write unsigned 8-bit integer */
    static void serializeUInt8(std::vector<uint8_t>& buffer, uint8_t value) {
        buffer.push_back(value);
    }
    /** @brief Read unsigned 8-bit integer */
    static uint8_t deserializeUInt8(const uint8_t* data, size_t& offset, size_t size) {
        if (offset + 1 > size) throw std::runtime_error("Insufficient data to read uint8_t");
        return data[offset++];
    }

    /** @brief Write signed 16-bit integer (big-endian) */
    static void serializeInt16(std::vector<uint8_t>& buffer, int16_t value) {
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back(value & 0xFF);
    }
    /** @brief Read signed 16-bit integer (big-endian) */
    static int16_t deserializeInt16(const uint8_t* data, size_t& offset, size_t size) {
        if (offset + 2 > size) throw std::runtime_error("Insufficient data to read int16_t");
        uint16_t v = (static_cast<uint16_t>(data[offset]) << 8) | static_cast<uint16_t>(data[offset + 1]);
        offset += 2;
        return static_cast<int16_t>(v);
    }

    /** @brief Write signed 32-bit integer (big-endian) */
    static void serializeInt32(std::vector<uint8_t>& buffer, int32_t value) {
        buffer.push_back((value >> 24) & 0xFF);
        buffer.push_back((value >> 16) & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back(value & 0xFF);
    }
    /** @brief Read signed 32-bit integer (big-endian) */
    static int32_t deserializeInt32(const uint8_t* data, size_t& offset, size_t size) {
        if (offset + 4 > size) throw std::runtime_error("Insufficient data to read int32_t");
        int32_t v = (static_cast<int32_t>(data[offset]) << 24) |
                    (static_cast<int32_t>(data[offset + 1]) << 16) |
                    (static_cast<int32_t>(data[offset + 2]) << 8) |
                    static_cast<int32_t>(data[offset + 3]);
        offset += 4;
        return v;
    }

    /** @brief Write signed 64-bit integer (big-endian) */
    static void serializeInt64(std::vector<uint8_t>& buffer, int64_t value) {
        buffer.push_back((value >> 56) & 0xFF);
        buffer.push_back((value >> 48) & 0xFF);
        buffer.push_back((value >> 40) & 0xFF);
        buffer.push_back((value >> 32) & 0xFF);
        buffer.push_back((value >> 24) & 0xFF);
        buffer.push_back((value >> 16) & 0xFF);
        buffer.push_back((value >> 8) & 0xFF);
        buffer.push_back(value & 0xFF);
    }
    /** @brief Read signed 64-bit integer (big-endian) */
    static int64_t deserializeInt64(const uint8_t* data, size_t& offset, size_t size) {
        if (offset + 8 > size) throw std::runtime_error("Insufficient data to read int64_t");
        int64_t v = (static_cast<int64_t>(data[offset]) << 56) |
                    (static_cast<int64_t>(data[offset + 1]) << 48) |
                    (static_cast<int64_t>(data[offset + 2]) << 40) |
                    (static_cast<int64_t>(data[offset + 3]) << 32) |
                    (static_cast<int64_t>(data[offset + 4]) << 24) |
                    (static_cast<int64_t>(data[offset + 5]) << 16) |
                    (static_cast<int64_t>(data[offset + 6]) << 8) |
                    static_cast<int64_t>(data[offset + 7]);
        offset += 8;
        return v;
    }

    /** @brief Write float as IEEE754 bits via 32-bit big-endian integer */
    static void serializeFloat(std::vector<uint8_t>& buffer, float value) {
        uint32_t iv;
        std::memcpy(&iv, &value, sizeof(float));
        serializeInt32(buffer, static_cast<int32_t>(iv));
    }
    /** @brief Read float from 32-bit big-endian integer (IEEE754) */
    static float deserializeFloat(const uint8_t* data, size_t& offset, size_t size) {
        int32_t iv = deserializeInt32(data, offset, size);
        float f;
        std::memcpy(&f, &iv, sizeof(float));
        return f;
    }

    // --------- Common header ---------
    /** @brief Serialize Header (frame_id, device_id, timestamp) */
    static void serializeHeader(std::vector<uint8_t>& buffer, const Header& header) {
        serializeString(buffer, header.frame_id);
        serializeString(buffer, header.device_id);
        serializeInt64(buffer, header.timestamp);
    }
    /** @brief Deserialize Header (order: frame_id, device_id, timestamp) */
    static Header deserializeHeader(const uint8_t* data, size_t& offset, size_t size) {
        Header h;
        h.frame_id = deserializeString(data, offset, size);
        h.device_id = deserializeString(data, offset, size);
        h.timestamp = deserializeInt64(data, offset, size);
        return h;
    }
    /**
     * @brief Serialize VitalData structure to binary format
     * @param data VitalData structure to serialize
     * @return Binary data as vector of bytes
     */
    static std::vector<uint8_t> serializeVitalData(const VitalData& data) {
        std::vector<uint8_t> buffer;
        
        // Serialize header
        serializeHeader(buffer, data.header);
        
        // Serialize vital data fields
        serializeInt32(buffer, data.init_stat);
        serializeInt32(buffer, data.maxd);
        serializeInt32(buffer, data.presence_status);
        serializeFloat(buffer, data.conf);
        serializeFloat(buffer, data.hr_est);
        serializeFloat(buffer, data.rr_est);
        serializeFloat(buffer, data.rr_amp);
        
        return buffer;
    }

    /**
     * @brief Deserialize VitalData structure from binary format
     * @param data Binary data buffer
     * @param size Size of the buffer
     * @return Deserialized VitalData structure
     */
    static VitalData deserializeVitalData(const uint8_t* data, size_t size) {
        VitalData vital_data;
        size_t offset = 0;
        
        try {
            // Deserialize header
            vital_data.header = deserializeHeader(data, offset, size);
            
            // Deserialize vital data fields
            vital_data.init_stat = deserializeInt32(data, offset, size);
            vital_data.maxd = deserializeInt32(data, offset, size);
            vital_data.presence_status = deserializeInt32(data, offset, size);
            vital_data.conf = deserializeFloat(data, offset, size);
            vital_data.hr_est = deserializeFloat(data, offset, size);
            vital_data.rr_est = deserializeFloat(data, offset, size);
            vital_data.rr_amp = deserializeFloat(data, offset, size);
            
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to deserialize VitalData: ") + e.what());
        }
        
        return vital_data;
    }

    /**
     * @brief Serialize LLMEmotionDetResult structure to binary format
     * @param data LLMEmotionDetResult structure to serialize
     * @return Binary data as vector of bytes
     */
    static std::vector<uint8_t> serializeLLMEmotionDetResult(const LLMEmotionDetResult& data) {
        std::vector<uint8_t> buffer;
        
        // Serialize header
        serializeHeader(buffer, data.header);
        
        // Serialize string fields
        serializeString(buffer, data.analysis_str);
        serializeString(buffer, data.analysis_id);
        
        // Serialize confidence
        serializeFloat(buffer, data.confidence);
        
        // Serialize emotions_labels array (7 strings)
        for (const auto& label : data.emotions_labels) {
            serializeString(buffer, label);
        }
        
        // Serialize emotions_probs array (7 floats)
        for (float prob : data.emotions_probs) {
            serializeFloat(buffer, prob);
        }
        
        return buffer;
    }

    /**
     * @brief Deserialize LLMEmotionDetResult structure from binary format
     * @param data Binary data buffer
     * @param size Size of the buffer
     * @return Deserialized LLMEmotionDetResult structure
     */
    static LLMEmotionDetResult deserializeLLMEmotionDetResult(const uint8_t* data, size_t size) {
        LLMEmotionDetResult result;
        size_t offset = 0;
        
        try {
            // Deserialize header
            result.header = deserializeHeader(data, offset, size);
            
            // Deserialize string fields
            result.analysis_str = deserializeString(data, offset, size);
            result.analysis_id = deserializeString(data, offset, size);
            
            // Deserialize confidence
            result.confidence = deserializeFloat(data, offset, size);
            
            // Deserialize emotions_labels array (7 strings)
            for (size_t i = 0; i < 7; ++i) {
                result.emotions_labels[i] = deserializeString(data, offset, size);
            }
            
            // Deserialize emotions_probs array (7 floats)
            for (size_t i = 0; i < 7; ++i) {
                result.emotions_probs[i] = deserializeFloat(data, offset, size);
            }
            
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to deserialize LLMEmotionDetResult: ") + e.what());
        }
        
        return result;
    }

    /**
     * @brief Serialize LLMEmotionIntention structure to binary format
     * @param data LLMEmotionIntention structure to serialize
     * @return Binary data as vector of bytes
     */
    static std::vector<uint8_t> serializeLLMEmotionIntention(const LLMEmotionIntention& data) {
        std::vector<uint8_t> buffer;
        
        // Serialize header
        serializeHeader(buffer, data.header);
        
        // Serialize string fields
        serializeString(buffer, data.intention_name);
        
        // Serialize confidence
        serializeFloat(buffer, data.confidence);
        
        // Serialize reasoning
        serializeString(buffer, data.reasoning);
        
        return buffer;
    }

    /**
     * @brief Deserialize LLMEmotionIntention structure from binary format
     * @param data Binary data buffer
     * @param size Size of the buffer
     * @return Deserialized LLMEmotionIntention structure
     */
    static LLMEmotionIntention deserializeLLMEmotionIntention(const uint8_t* data, size_t size) {
        LLMEmotionIntention result;
        size_t offset = 0;
        
        try {
            // Deserialize header
            result.header = deserializeHeader(data, offset, size);
            
            // Deserialize intention_name
            result.intention_name = deserializeString(data, offset, size);
            
            // Deserialize confidence
            result.confidence = deserializeFloat(data, offset, size);
            
            // Deserialize reasoning
            result.reasoning = deserializeString(data, offset, size);
            
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to deserialize LLMEmotionIntention: ") + e.what());
        }
        
        return result;
    }

    // --------- BUTTON_STATUS ---------

    // --------- BUTTON_STATUS_EVENT ---------
    /**
     * Serialize ButtonStatusEventMsg
     * Field order: Header, button_id(i32), button_stat(i32)
     */
    static std::vector<uint8_t> serializeButtonStatusEvent(const ButtonStatusEventMsg& m) {
        std::vector<uint8_t> buffer;
        serializeHeader(buffer, m.header);
        serializeInt32(buffer, m.button_id);
        serializeInt32(buffer, m.button_stat);
        return buffer;
    }

    /**
     * Deserialize ButtonStatusEventMsg
     */
    static ButtonStatusEventMsg deserializeButtonStatusEvent(const uint8_t* data, size_t size) {
        ButtonStatusEventMsg m{};
        size_t offset = 0;
        m.header = deserializeHeader(data, offset, size);
        m.button_id = deserializeInt32(data, offset, size);
        m.button_stat = deserializeInt32(data, offset, size);
        return m;
    }

    // --------- VITAL_VIS_DATA ---------
    /**
     * @brief Serialize VitalVisData structure to binary format
     * Field order: Header, ppg_waveform[256](float), breath_waveform[256](float)
     */
    static std::vector<uint8_t> serializeVitalVisData(const VitalVisData& m) {
        std::vector<uint8_t> buffer;
        serializeHeader(buffer, m.header);
        
        // Serialize ppg_waveform array (256 floats)
        for (int i = 0; i < 256; ++i) {
            serializeFloat(buffer, m.ppg_waveform[i]);
        }
        
        // Serialize breath_waveform array (256 floats)
        for (int i = 0; i < 256; ++i) {
            serializeFloat(buffer, m.breath_waveform[i]);
        }
        
        return buffer;
    }

    /**
     * @brief Deserialize VitalVisData structure from binary format
     */
    static VitalVisData deserializeVitalVisData(const uint8_t* data, size_t size) {
        VitalVisData m{};
        size_t offset = 0;
        
        m.header = deserializeHeader(data, offset, size);
        
        // Deserialize ppg_waveform array (256 floats)
        for (int i = 0; i < 256; ++i) {
            m.ppg_waveform[i] = deserializeFloat(data, offset, size);
        }
        
        // Deserialize breath_waveform array (256 floats)
        for (int i = 0; i < 256; ++i) {
            m.breath_waveform[i] = deserializeFloat(data, offset, size);
        }
        
        return m;
    }

    // --------- HEARTBEAT (id=1) ---------
    /**
     * @brief Serialize Heartbeat message
     * Field order: Header, hw_version(u8), sw_version(u8), timestamp_ns(i32)
     */
    static std::vector<uint8_t> serializeHeartbeat(const HeartbeatMsg& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeUInt8(buf, m.hw_version);
        serializeUInt8(buf, m.sw_version);
        serializeInt32(buf, static_cast<int32_t>(m.timestamp_ns));
        return buf;
    }
    /** @brief Deserialize Heartbeat (same field order as serialize) */
    static HeartbeatMsg deserializeHeartbeat(const uint8_t* data, size_t size) {
        HeartbeatMsg m{}; size_t off = 0;
        m.header = deserializeHeader(data, off, size);
        m.hw_version = deserializeUInt8(data, off, size);
        m.sw_version = deserializeUInt8(data, off, size);
        m.timestamp_ns = static_cast<uint32_t>(deserializeInt32(data, off, size));
        return m;
    }

    // --------- A_MOTOR_CONTROL (id=10) ---------
    /**
     * @brief Serialize motor control
     * Field order: Header, position[8](i16, big-endian), enable_mask(u8)
     */
    static std::vector<uint8_t> serializeMotorControl(const MotorControlMsg& m) {
        std::vector<uint8_t> buf; serializeHeader(buf, m.header);
        for (int16_t v : m.position) serializeInt16(buf, v);
        serializeUInt8(buf, m.enable_mask);
        return buf;
    }
    /** @brief Deserialize motor control (same field order as serialize) */
    static MotorControlMsg deserializeMotorControl(const uint8_t* data, size_t size) {
        MotorControlMsg m{}; size_t off = 0; m.header = deserializeHeader(data, off, size);
        for (auto& v : m.position) v = deserializeInt16(data, off, size);
        m.enable_mask = deserializeUInt8(data, off, size);
        return m;
    }

    // --------- A_TEM_CONTROL (id=11) ---------
    /** @brief Serialize temperature control: Header, temperature(u8) */
    static std::vector<uint8_t> serializeTempControl(const TempControlMsg& m) {
        std::vector<uint8_t> buf; serializeHeader(buf, m.header);
        serializeUInt8(buf, m.temperature);
        return buf;
    }
    /** @brief Deserialize temperature control */
    static TempControlMsg deserializeTempControl(const uint8_t* data, size_t size) {
        TempControlMsg m{}; size_t off = 0; m.header = deserializeHeader(data, off, size);
        m.temperature = deserializeUInt8(data, off, size);
        return m;
    }

    // --------- A_PWR_CONTROL (id=12) ---------
    /** @brief Serialize power control: Header, state(u8), time_s(i32) */
    static std::vector<uint8_t> serializePowerControl(const PowerControlMsg& m) {
        std::vector<uint8_t> buf; serializeHeader(buf, m.header);
        serializeUInt8(buf, static_cast<uint8_t>(m.state));
        serializeInt32(buf, static_cast<int32_t>(m.time_s));
        return buf;
    }
    /** @brief Deserialize power control */
    static PowerControlMsg deserializePowerControl(const uint8_t* data, size_t size) {
        PowerControlMsg m{}; size_t off = 0; m.header = deserializeHeader(data, off, size);
        m.state = static_cast<APwrRequest>(deserializeUInt8(data, off, size));
        m.time_s = static_cast<uint32_t>(deserializeInt32(data, off, size));
        return m;
    }

    // --------- B_TOUCH_STATUS RAW (id=21, 原始B板数据) ---------
    /** @brief Serialize raw touch status: Header, touch_id_mask(u8), error_mask(u8), slave_timestamp_ms(u32) */
    static std::vector<uint8_t> serializeRawTouchStatus(const RawTouchStatusMsg& m) {
        std::vector<uint8_t> buf; 
        serializeHeader(buf, m.header);
        serializeUInt8(buf, m.touch_id_mask);
        serializeUInt8(buf, m.error_mask);
        serializeInt32(buf, static_cast<int32_t>(m.slave_timestamp_ms));
        return buf;
    }
    /** @brief Deserialize raw touch status */
    static RawTouchStatusMsg deserializeRawTouchStatus(const uint8_t* data, size_t size) {
        RawTouchStatusMsg m{}; 
        size_t off = 0; 
        m.header = deserializeHeader(data, off, size);
        m.touch_id_mask = deserializeUInt8(data, off, size);
        m.error_mask = deserializeUInt8(data, off, size);
        m.slave_timestamp_ms = static_cast<uint32_t>(deserializeInt32(data, off, size));
        return m;
    }

    // --------- B_TOUCH_EVENT RAW (id=27, 触摸事件) ---------
    /** @brief Serialize raw touch event: Header, touch_id(u8), new_state(u8), event_id(u32), slave_timestamp_ms(u32), timestamp_last_state_ms(u32) */
    static std::vector<uint8_t> serializeRawTouchEvent(const RawTouchEventMsg& m) {
        std::vector<uint8_t> buf; 
        serializeHeader(buf, m.header);
        serializeUInt8(buf, m.touch_id);
        serializeUInt8(buf, m.new_state);
        serializeInt32(buf, static_cast<int32_t>(m.event_id));
        serializeInt32(buf, static_cast<int32_t>(m.slave_timestamp_ms));
        serializeInt32(buf, static_cast<int32_t>(m.timestamp_last_state_ms));
        return buf;
    }
    /** @brief Deserialize raw touch event */
    static RawTouchEventMsg deserializeRawTouchEvent(const uint8_t* data, size_t size) {
        RawTouchEventMsg m{}; 
        size_t off = 0; 
        m.header = deserializeHeader(data, off, size);
        m.touch_id = deserializeUInt8(data, off, size);
        m.new_state = deserializeUInt8(data, off, size);
        m.event_id = static_cast<uint32_t>(deserializeInt32(data, off, size));
        m.slave_timestamp_ms = static_cast<uint32_t>(deserializeInt32(data, off, size));
        m.timestamp_last_state_ms = static_cast<uint32_t>(deserializeInt32(data, off, size));
        return m;
    }

    // --------- TOUCH_STATUS (处理后的触摸状态) ---------
    /** @brief Serialize touch status: Header, error_mask(u8), panel_status[4](u8), panel_last_idle_stamp_ns[4](u64), panel_event_duration_ms[4](u64), panel_event_id[4](i32) */
    static std::vector<uint8_t> serializeTouchStatus(const TouchStatusMsg& m) {
        std::vector<uint8_t> buf; 
        serializeHeader(buf, m.header);
        serializeUInt8(buf, m.error_mask);
        for (uint8_t status : m.panel_status) {
            serializeUInt8(buf, status);
        }
        for (uint64_t ts : m.panel_last_idle_stamp_ns) {
            serializeInt64(buf, static_cast<int64_t>(ts));
        }
        for (uint64_t duration : m.panel_event_duration_ms) {
            serializeInt64(buf, static_cast<int64_t>(duration));
        }
        for (int32_t id : m.panel_event_id) {
            serializeInt32(buf, id);
        }
        return buf;
    }
    /** @brief Deserialize touch status */
    static TouchStatusMsg deserializeTouchStatus(const uint8_t* data, size_t size) {
        TouchStatusMsg m{}; 
        size_t off = 0; 
        m.header = deserializeHeader(data, off, size);
        m.error_mask = deserializeUInt8(data, off, size);
        for (auto& status : m.panel_status) {
            status = deserializeUInt8(data, off, size);
        }
        for (auto& ts : m.panel_last_idle_stamp_ns) {
            ts = static_cast<uint64_t>(deserializeInt64(data, off, size));
        }
        for (auto& duration : m.panel_event_duration_ms) {
            duration = static_cast<uint64_t>(deserializeInt64(data, off, size));
        }
        for (auto& id : m.panel_event_id) {
            id = deserializeInt32(data, off, size);
        }
        return m;
    }

    // --------- B_MOTOR_STATUS (id=22) ---------
    /** @brief Serialize motor status: Header, position[8](i16), enable(u8), block(u8), error(u8) */
    static std::vector<uint8_t> serializeMotorStatus(const MotorStatusMsg& m) {
        std::vector<uint8_t> buf; serializeHeader(buf, m.header);
        for (int16_t v : m.position) serializeInt16(buf, v);
        serializeUInt8(buf, m.motor_enable_mask);
        serializeUInt8(buf, m.motor_block_mask);
        serializeUInt8(buf, m.motor_error_mask);
        return buf;
    }
    /** @brief Deserialize motor status */
    static MotorStatusMsg deserializeMotorStatus(const uint8_t* data, size_t size) {
        MotorStatusMsg m{}; size_t off = 0; m.header = deserializeHeader(data, off, size);
        for (auto& v : m.position) v = deserializeInt16(data, off, size);
        m.motor_enable_mask = deserializeUInt8(data, off, size);
        m.motor_block_mask = deserializeUInt8(data, off, size);
        m.motor_error_mask = deserializeUInt8(data, off, size);
        return m;
    }

    // --------- B_TEM_STATUS (id=23) ---------
    /** @brief Serialize temperature status: Header, temperature(u8), tmp_error(u8) */
    static std::vector<uint8_t> serializeTempStatus(const TempStatusMsg& m) {
        std::vector<uint8_t> buf; serializeHeader(buf, m.header);
        serializeUInt8(buf, m.temperature);
        serializeUInt8(buf, static_cast<uint8_t>(m.tmp_error));
        return buf;
    }
    /** @brief Deserialize temperature status */
    static TempStatusMsg deserializeTempStatus(const uint8_t* data, size_t size) {
        TempStatusMsg m{}; size_t off = 0; m.header = deserializeHeader(data, off, size);
        m.temperature = deserializeUInt8(data, off, size);
        m.tmp_error = static_cast<BTempErrorState>(deserializeUInt8(data, off, size));
        return m;
    }

    // --------- B_IMU_STATUS (id=24) ---------
    /** @brief Serialize IMU status: Header, state(u8), quaternion[4](float), acceleration[3](float), angular_velocity[3](float) */
    static std::vector<uint8_t> serializeImuStatus(const ImuStatusMsg& m) {
        std::vector<uint8_t> buf; serializeHeader(buf, m.header);
        serializeUInt8(buf, static_cast<uint8_t>(m.state));
        for (float f : m.quaternion) serializeFloat(buf, f);
        for (float f : m.acceleration) serializeFloat(buf, f);
        for (float f : m.angular_velocity) serializeFloat(buf, f);
        return buf;
    }
    /** @brief Deserialize IMU status */
    static ImuStatusMsg deserializeImuStatus(const uint8_t* data, size_t size) {
        ImuStatusMsg m{}; size_t off = 0; m.header = deserializeHeader(data, off, size);
        m.state = static_cast<BImuState>(deserializeUInt8(data, off, size));
        for (auto& f : m.quaternion) f = deserializeFloat(data, off, size);
        for (auto& f : m.acceleration) f = deserializeFloat(data, off, size);
        for (auto& f : m.angular_velocity) f = deserializeFloat(data, off, size);
        return m;
    }

    // --------- B_SYS_STATUS (id=25) ---------
    /** @brief Serialize system status: Header, pwr_off(u8), charge_state(u8), battery_level(u8) */
    static std::vector<uint8_t> serializeSysStatus(const SysStatusMsg& m) {
        std::vector<uint8_t> buf; serializeHeader(buf, m.header);
        serializeUInt8(buf, m.pwr_off);
        serializeUInt8(buf, static_cast<uint8_t>(m.charge_state));
        serializeUInt8(buf, m.battery_level);
        return buf;
    }
    /** @brief Deserialize system status */
    static SysStatusMsg deserializeSysStatus(const uint8_t* data, size_t size) {
        SysStatusMsg m{}; size_t off = 0; m.header = deserializeHeader(data, off, size);
        m.pwr_off = deserializeUInt8(data, off, size);
        m.charge_state = static_cast<BChargeState>(deserializeUInt8(data, off, size));
        m.battery_level = deserializeUInt8(data, off, size);
        return m;
    }

    // --------- VISUAL_FEATURE_FRAME ---------
    /** 
     * @brief Serialize VisualFeatureFrame
     * Field order: Header, frame_index(i32), faces_size(i32), faces[](float*5), 
     *              macro_expression[7](float), face_heading_yaw(float), face_heading_pitch(float)
     */
    static std::vector<uint8_t> serializeVisualFeature(const VisualFeatureFrame& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeInt32(buf, m.frame_index);
        
        // Serialize faces vector
        serializeInt32(buf, static_cast<int32_t>(m.faces.size()));
        for (const auto& face : m.faces) {
            for (float val : face) {
                serializeFloat(buf, val);
            }
        }
        
        // Serialize macro_expression array
        for (float val : m.macro_expression) {
            serializeFloat(buf, val);
        }
        
        // Serialize face heading orientation (degrees)
        serializeFloat(buf, m.face_heading_yaw);
        serializeFloat(buf, m.face_heading_pitch);
        
        return buf;
    }
    
    /** @brief Deserialize VisualFeatureFrame */
    static VisualFeatureFrame deserializeVisualFeature(const uint8_t* data, size_t size) {
        VisualFeatureFrame m{};
        size_t off = 0;
        
        m.header = deserializeHeader(data, off, size);
        m.frame_index = deserializeInt32(data, off, size);
        
        // Deserialize faces vector
        int32_t faces_count = deserializeInt32(data, off, size);
        m.faces.resize(faces_count);
        for (int32_t i = 0; i < faces_count; ++i) {
            for (int j = 0; j < 5; ++j) {
                m.faces[i][j] = deserializeFloat(data, off, size);
            }
        }
        
        // Deserialize macro_expression array
        for (int i = 0; i < 7; ++i) {
            m.macro_expression[i] = deserializeFloat(data, off, size);
        }
        
        // Deserialize face heading orientation (degrees)
        m.face_heading_yaw = deserializeFloat(data, off, size);
        m.face_heading_pitch = deserializeFloat(data, off, size);
        
        return m;
    }

    // --------- EYEBALL_DISPLAY_COMMAND ---------
    /**
     * @brief Serialize EyeballDispalyCommand
     * Field order: Header, eyeball_id(i32), blink_rate(float), file_path_count(i32), file_path[n](string), position.x(i16), position.y(i16)
     */
    static std::vector<uint8_t> serializeEyeballDisplayCommand(const EyeballDispalyCommand& m) {
        std::vector<uint8_t> buffer;
        serializeHeader(buffer, m.header);
        serializeInt32(buffer, static_cast<int32_t>(m.eyeball_id));
        serializeFloat(buffer, m.blink_rate);
        serializeInt32(buffer, static_cast<int32_t>(m.file_path.size()));
        for (const auto& s : m.file_path) {
            serializeString(buffer, s);
        }
        serializeUInt8(buffer, static_cast<uint8_t>(m.enable_tracking ? 1 : 0));
        return buffer;
    }
    
    /** @brief Deserialize EyeballDispalyCommand (same field order as serialize) */
    static EyeballDispalyCommand deserializeEyeballDisplayCommand(const uint8_t* data, size_t size) {
        size_t offset = 0;
        EyeballDispalyCommand m{};
        m.header = deserializeHeader(data, offset, size);
        m.eyeball_id = deserializeInt32(data, offset, size);
        m.blink_rate = deserializeFloat(data, offset, size);
        int32_t count = deserializeInt32(data, offset, size);
        if (count < 0) throw std::runtime_error("negative file_path count");
        m.file_path.clear();
        m.file_path.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i) {
            m.file_path.emplace_back(deserializeString(data, offset, size));
        }
        m.enable_tracking = (deserializeUInt8(data, offset, size) != 0);
        return m;
    }

    // --------- AUDIO_PLAY_COMMAND ---------
    /** 
     * @brief Serialize AudioPlayCommand
     * Field order: Header, file_path(string), speed(float), volume(float), loop(u8)
     */
    static std::vector<uint8_t> serializeAudioPlayCommand(const AudioPlayCommand& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeString(buf, m.file_path);
        serializeFloat(buf, m.speed);
        serializeFloat(buf, m.volume);
        serializeUInt8(buf, static_cast<uint8_t>(m.loop ? 1 : 0));
        return buf;
    }
    
    /** @brief Deserialize AudioPlayCommand */
    static AudioPlayCommand deserializeAudioPlayCommand(const uint8_t* data, size_t size) {
        AudioPlayCommand m{};
        size_t off = 0;
        
        m.header = deserializeHeader(data, off, size);
        m.file_path = deserializeString(data, off, size);
        m.speed = deserializeFloat(data, off, size);
        m.volume = deserializeFloat(data, off, size);
        m.loop = (deserializeUInt8(data, off, size) != 0);
        
        return m;
    }

    // --------- LED_CONTROL_MSG ---------
    /**
     * @brief Serialize LedControlMsg
     * Field order: Header, r(u8), g(u8), b(u8), brightness(float)
     */
    static std::vector<uint8_t> serializeLedControl(const LedControlMsg& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeUInt8(buf, m.r);
        serializeUInt8(buf, m.g);
        serializeUInt8(buf, m.b);
        serializeFloat(buf, m.brightness);
        return buf;
    }

    /** @brief Deserialize LedControlMsg */
    static LedControlMsg deserializeLedControl(const uint8_t* data, size_t size) {
        LedControlMsg m{};
        size_t off = 0;
        m.header = deserializeHeader(data, off, size);
        m.r = deserializeUInt8(data, off, size);
        m.g = deserializeUInt8(data, off, size);
        m.b = deserializeUInt8(data, off, size);
        m.brightness = deserializeFloat(data, off, size);
        return m;
    }

    // --------- ACTION_GROUP_EXECUTE_COMMAND ---------
    /** 
     * @brief Serialize ActionGroupExecuteCommand
     * Field order: Header, action_name(string), speed_scale(float), blocking(u8),
     *              motor_configs_count(i32), motor_configs[](motor_idx, controller_type, param_count, params[])
     *              duration_ms(i32), idle_time_ms(i32), loop(u8),
     *              steps_count(i32), steps[](name, duration_ms, idle_time_ms, loop, step_motor_count, step_motors[])
     */
    static std::vector<uint8_t> serializeActionGroupExecuteCommand(const ActionGroupExecuteCommand& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeString(buf, m.action_name);
        serializeFloat(buf, m.speed_scale);
        serializeUInt8(buf, static_cast<uint8_t>(m.blocking ? 1 : 0));
        
        // 优先序列化新的motor_configs结构
        if (!m.motor_configs.empty()) {
            // 序列化motor配置数量
            serializeInt32(buf, static_cast<int32_t>(m.motor_configs.size()));
            
            // 序列化每个motor配置
            for (const auto& config : m.motor_configs) {
                serializeInt32(buf, config.motor_idx);
                serializeUInt8(buf, static_cast<uint8_t>(config.controller_type));
                serializeInt32(buf, static_cast<int32_t>(config.parameters.size()));
                
                for (float param : config.parameters) {
                    serializeFloat(buf, param);
                }
            }
        } else {
            // 向后兼容：如果motor_configs为空，序列化旧的parameters格式
            // 使用特殊标记 -1 表示使用旧格式
            serializeInt32(buf, -1);
            serializeInt32(buf, static_cast<int32_t>(m.parameters.size()));
            for (float param : m.parameters) {
                serializeFloat(buf, param);
            }
        }

        // 扩展字段：duration/idle/loop
        serializeInt32(buf, m.duration_ms);
        serializeInt32(buf, m.idle_time_ms);
        serializeUInt8(buf, static_cast<uint8_t>(m.loop ? 1 : 0));

        // 扩展字段：步骤序列
        serializeInt32(buf, static_cast<int32_t>(m.steps.size()));
        for (const auto& step : m.steps) {
            serializeString(buf, step.name);
            serializeInt32(buf, step.duration_ms);
            serializeInt32(buf, step.idle_time_ms);
            serializeUInt8(buf, static_cast<uint8_t>(step.loop ? 1 : 0));

            serializeInt32(buf, static_cast<int32_t>(step.motor_configs.size()));
            for (const auto& config : step.motor_configs) {
                serializeInt32(buf, config.motor_idx);
                serializeUInt8(buf, static_cast<uint8_t>(config.controller_type));
                serializeInt32(buf, static_cast<int32_t>(config.parameters.size()));

                for (float param : config.parameters) {
                    serializeFloat(buf, param);
                }
            }
        }
        
        return buf;
    }
    
    /** @brief Deserialize ActionGroupExecuteCommand */
    static ActionGroupExecuteCommand deserializeActionGroupExecuteCommand(const uint8_t* data, size_t size) {
        ActionGroupExecuteCommand m{};
        size_t off = 0;
        
        m.header = deserializeHeader(data, off, size);
        m.action_name = deserializeString(data, off, size);
        m.speed_scale = deserializeFloat(data, off, size);
        m.blocking = (deserializeUInt8(data, off, size) != 0);
        
        // 读取motor配置数量（或旧格式标记）
        int32_t motor_count = deserializeInt32(data, off, size);
        
        if (motor_count == -1) {
            // 旧格式：直接读取parameters数组
            int32_t param_count = deserializeInt32(data, off, size);
            m.parameters.resize(param_count);
            std::cout << "[Deserialize] Old format, param_count: " << param_count << std::endl;
            for (int32_t i = 0; i < param_count; ++i) {
                m.parameters[i] = deserializeFloat(data, off, size);
            }
        } else {
            // 新格式：读取motor_configs数组
            std::cout << "[Deserialize] New format, motor_count: " << motor_count << std::endl;
            m.motor_configs.resize(motor_count);
            
            for (int32_t i = 0; i < motor_count; ++i) {
                auto& config = m.motor_configs[i];
                config.motor_idx = deserializeInt32(data, off, size);
                config.controller_type = static_cast<MotorControllerType>(deserializeUInt8(data, off, size));
                
                int32_t param_count = deserializeInt32(data, off, size);
                config.parameters.resize(param_count);
                
                for (int32_t j = 0; j < param_count; ++j) {
                    config.parameters[j] = deserializeFloat(data, off, size);
                }
                
                std::cout << "  Motor " << config.motor_idx 
                          << ", Controller: " << static_cast<int>(config.controller_type)
                          << ", Params: " << param_count << std::endl;
            }
        }

        // 尝试读取扩展字段，若数据不足则保持默认值
        auto safeRemaining = [&](size_t need) { return (off + need) <= size; };
        if (safeRemaining(sizeof(int32_t))) {
            m.duration_ms = deserializeInt32(data, off, size);
        }
        if (safeRemaining(sizeof(int32_t))) {
            m.idle_time_ms = deserializeInt32(data, off, size);
        }
        if (safeRemaining(sizeof(uint8_t))) {
            m.loop = deserializeUInt8(data, off, size) != 0;
        }

        // 步骤序列（可选）
        if (safeRemaining(sizeof(int32_t))) {
            int32_t step_count = deserializeInt32(data, off, size);
            if (step_count > 0 && safeRemaining(0)) {
                m.steps.resize(step_count);
                for (int32_t i = 0; i < step_count; ++i) {
                    auto& step = m.steps[i];
                    step.name = deserializeString(data, off, size);
                    if (safeRemaining(sizeof(int32_t))) step.duration_ms = deserializeInt32(data, off, size);
                    if (safeRemaining(sizeof(int32_t))) step.idle_time_ms = deserializeInt32(data, off, size);
                    if (safeRemaining(sizeof(uint8_t))) step.loop = deserializeUInt8(data, off, size) != 0;

                    int32_t step_motor_count = safeRemaining(sizeof(int32_t)) ? deserializeInt32(data, off, size) : 0;
                    if (step_motor_count > 0) {
                        step.motor_configs.resize(step_motor_count);
                        for (int32_t j = 0; j < step_motor_count; ++j) {
                            auto& config = step.motor_configs[j];
                            config.motor_idx = deserializeInt32(data, off, size);
                            config.controller_type = static_cast<MotorControllerType>(deserializeUInt8(data, off, size));
                            int32_t param_count = deserializeInt32(data, off, size);
                            config.parameters.resize(param_count);
                            for (int32_t k = 0; k < param_count; ++k) {
                                config.parameters[k] = deserializeFloat(data, off, size);
                            }
                        }
                    }
                }
            }
        }
        
        return m;
    }

    // ---------------- ADTS STREAM ----------------
    /**
     * @brief Serialize AdtsStreamControlMsg
     * Field order: Header, is_start(u8), sample_rate(i32), channels(u8), bit_rate(i32), aot(u8)
     */
    static std::vector<uint8_t> serializeAdtsStreamControl(const AdtsStreamControlMsg& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeUInt8(buf, static_cast<uint8_t>(m.is_start ? 1 : 0));
        serializeInt32(buf, static_cast<int32_t>(m.sample_rate));
        serializeUInt8(buf, m.channels);
        serializeInt32(buf, static_cast<int32_t>(m.bit_rate));
        serializeUInt8(buf, m.aot);
        return buf;
    }

    /** @brief Deserialize AdtsStreamControlMsg */
    static AdtsStreamControlMsg deserializeAdtsStreamControl(const uint8_t* data, size_t size) {
        AdtsStreamControlMsg m{};
        size_t off = 0;
        m.header = deserializeHeader(data, off, size);
        m.is_start = (deserializeUInt8(data, off, size) != 0);
        m.sample_rate = static_cast<uint32_t>(deserializeInt32(data, off, size));
        m.channels = deserializeUInt8(data, off, size);
        m.bit_rate = static_cast<uint32_t>(deserializeInt32(data, off, size));
        m.aot = deserializeUInt8(data, off, size);
        return m;
    }

    /**
     * @brief Serialize AdtsStreamDataMsg
     * Field order: Header, seq(i32), pts_ms(i64), frame_count(i16), payload_len(i32), payload(bytes)
     */
    static std::vector<uint8_t> serializeAdtsStreamData(const AdtsStreamDataMsg& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeInt32(buf, static_cast<int32_t>(m.seq));
        serializeInt64(buf, static_cast<int64_t>(m.pts_ms));
        serializeInt16(buf, static_cast<int16_t>(m.frame_count));
        const uint32_t plen = static_cast<uint32_t>(m.payload.size());
        serializeInt32(buf, static_cast<int32_t>(plen));
        buf.insert(buf.end(), m.payload.begin(), m.payload.end());
        return buf;
    }

    /** @brief Deserialize AdtsStreamDataMsg */
    static AdtsStreamDataMsg deserializeAdtsStreamData(const uint8_t* data, size_t size) {
        AdtsStreamDataMsg m{};
        size_t off = 0;
        m.header = deserializeHeader(data, off, size);
        m.seq = static_cast<uint32_t>(deserializeInt32(data, off, size));
        m.pts_ms = static_cast<uint64_t>(deserializeInt64(data, off, size));
        m.frame_count = static_cast<uint16_t>(deserializeInt16(data, off, size));
        uint32_t plen = static_cast<uint32_t>(deserializeInt32(data, off, size));
        if (off + plen > size) throw std::runtime_error("Insufficient data to read AdtsStreamData payload");
        m.payload.assign(data + off, data + off + plen);
        off += plen;
        return m;
    }

    // --------- SYSTEM_STAT_INFO ---------
    /**
     * @brief Serialize SystemStatInfo
     * Field order: Header, cat_pad_values[3](float), cat_trust_value(float)
     */
    static std::vector<uint8_t> serializeSystemStatInfo(const SystemStatInfo& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        
        // Serialize cat_pad_values array (3 floats)
        for (float val : m.cat_pad_values) {
            serializeFloat(buf, val);
        }
        
        // Serialize cat_trust_value
        serializeFloat(buf, m.cat_trust_value);
        
        return buf;
    }

    /** @brief Deserialize SystemStatInfo */
    static SystemStatInfo deserializeSystemStatInfo(const uint8_t* data, size_t size) {
        SystemStatInfo m{};
        size_t off = 0;
        
        m.header = deserializeHeader(data, off, size);
        
        // Deserialize cat_pad_values array (3 floats)
        for (size_t i = 0; i < 3; ++i) {
            m.cat_pad_values[i] = deserializeFloat(data, off, size);
        }
        
        // Deserialize cat_trust_value
        m.cat_trust_value = deserializeFloat(data, off, size);
        
        return m;
    }

    // ---------------- SOUND LOCALIZATION ----------------
    /**
     * Serialize SoundLocalizationMsg
     * Field order: Header, azimuth_deg(float), elevation_deg(float), confidence(float), loudness(float)
     */
    static std::vector<uint8_t> serializeSoundLocalization(const SoundLocalizationMsg& m) {
        std::vector<uint8_t> buf;
        serializeHeader(buf, m.header);
        serializeFloat(buf, m.azimuth_deg);
        serializeFloat(buf, m.elevation_deg);
        serializeFloat(buf, m.confidence);
        for(float val : m.loudness){
            serializeFloat(buf, val);
        }
        return buf;
    }

    /**
     * Deserialize SoundLocalizationMsg
     */
    static SoundLocalizationMsg deserializeSoundLocalization(const uint8_t* data, size_t size) {
        SoundLocalizationMsg m{};
        size_t off = 0;
        m.header = deserializeHeader(data, off, size);
        m.azimuth_deg = deserializeFloat(data, off, size);
        m.elevation_deg = deserializeFloat(data, off, size);
        m.confidence = deserializeFloat(data, off, size);
        for(auto& val : m.loudness){
            val = deserializeFloat(data, off, size);
        }
        return m;
    }
};

} // namespace MsgsSerializer
} // namespace BionicCat

#endif // BIONIC_CAT_MSGS_SERIALIZER_HPP
