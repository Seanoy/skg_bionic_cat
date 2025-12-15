#ifndef BIONIC_CAT_MQTT_MSG_HPP
#define BIONIC_CAT_MQTT_MSG_HPP

#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace BionicCat{
namespace MqttMsgs {

// 统一的消息头（MQTT转发时可直接复用）
struct Header {
    std::string frame_id;   // 例如 "catlink"
    std::string device_id;  // 设备ID，例如 A/B 板序列号
    int64_t timestamp;      // 微秒时间戳
};

//====================
// 枚举（与 catlink.xml 对齐）
//====================

enum class RobotId : uint8_t { A = 1, B = 2 };

// A_PWR_REQUEST
enum class APwrRequest : uint8_t {
    NORMAL = 1,       // A_PWR_NORMAL
    OFF = 2,          // A_PWR_OFF（10s 后关机）
    REOPEN = 3,       // A_PWR_REOPEN（10s 后关机，并在 time_s 秒后重启）
    NO_SHUTDOWN = 4   // A_PWR_NO_SHUTDOWN（禁止关机）
};

// B_IMU_STATE
enum class BImuState : uint8_t {
    UPRIGHT = 0,
    TILT = 1,
    SHAKE = 2,
    PAT = 3,
    FALL = 4,
    ERROR = 16
};

// B_CHARGE_STATE
enum class BChargeState : uint8_t {
    NONE = 0,
    CHARGING = 2,
    FULL = 3,
    ERROR = 16
};

// B_TMP_ERROR_STATE
enum class BTempErrorState : uint8_t {
    NORMAL = 0,
    SENSOR_ERROR = 1,
    HEATER_ERROR = 2
};

//====================
// A -> B（A板发送）
//====================

// HEARTBEAT (id=1)
struct HeartbeatMsg {
    Header header;
    uint8_t hw_version = 0;      // 硬件版本
    uint8_t sw_version = 0;      // 软件版本
    uint32_t timestamp_ns = 0;   // 系统时间戳（可与 header.timestamp 保持一致）
};

// A_MOTOR_CONTROL (id=10)
struct MotorControlMsg {
    Header header;
    std::array<int16_t, 8> position{}; // 100*dgree，对应 -18000~18000
    uint8_t enable_mask = 0;           // bit0~bit7 对应电机0~7
};

// A_TEM_CONTROL (id=11)
struct TempControlMsg {
    Header header;
    uint8_t temperature = 0; // 0-40，0=不加热
};

// A_PWR_CONTROL (id=12)
struct PowerControlMsg {
    Header header;
    APwrRequest state = APwrRequest::NORMAL;
    uint32_t time_s = 0; // 仅在 state=REOPEN 时有效
};

//====================
// B -> A（A板接收）
//====================

// B_TOUCH_STATUS (id=21) - 与mavlink消息保持一致
struct RawTouchStatusMsg {
    Header header;
    uint8_t touch_id_mask = 0;   // bit0~bit4 表示触摸点, refer to enum B_TOUCH_ID
    uint8_t error_mask = 0;      // bit0~bit4 传感器异常
    uint32_t slave_timestamp_ms = 0; // B板事件时间戳 (milliseconds)
};

// B_TOUCH_EVENT (id=27) - 触摸事件消息
struct RawTouchEventMsg {
    Header header;
    uint8_t touch_id = 0;                // 触摸点ID, refer to enum B_TOUCH_ID
    uint8_t new_state = 0;               // 新状态 (1=touch active, 0=not touched)
    uint32_t event_id = 0;               // 事件ID
    uint32_t slave_timestamp_ms = 0;     // B板事件时间戳 (milliseconds)
    uint32_t timestamp_last_state_ms = 0; // 上一次状态改变时间戳 (milliseconds)
};

enum class TouchPanelInteractionStatus : uint8_t {
    IDLE = 0,
    TOUCHED = 1,
    PETTING = 2,
};

struct TouchStatusMsg {
    Header header;
    uint8_t error_mask = 0; // bit0~bit4 传感器异常
    std::array<uint8_t, 4> panel_status;
    std::array<uint64_t, 4> panel_last_idle_stamp_ns;  // 每个面板上次从非IDLE切换到IDLE的时间戳（纳秒）
    std::array<uint64_t, 4> panel_event_duration_ms;   // 每个面板事件的持续时间（毫秒）
    std::array<int32_t, 4> panel_event_id;             // 每个面板事件的唯一ID
};

// B_MOTOR_STATUS (id=22)
struct MotorStatusMsg {
    Header header;
    std::array<int16_t, 8> position{}; // 实际角度，100*dgree
    uint8_t motor_enable_mask = 0;
    uint8_t motor_block_mask = 0;
    uint8_t motor_error_mask = 0;
};

// B_TEM_STATUS (id=23)
struct TempStatusMsg {
    Header header;
    uint8_t temperature = 0;                 // bit0-6 温度，bit7 是否加热
    BTempErrorState tmp_error = BTempErrorState::NORMAL; // 异常枚举
};

// B_IMU_STATUS (id=24)
struct ImuStatusMsg {
    Header header;
    BImuState state = BImuState::ERROR;          // 255=传感器失效（如需兼容可扩展）
    std::array<float, 4> quaternion{0,0,0,1};    // w,x,y,z
    std::array<float, 3> acceleration{0,0,0};    // m/s^2
    std::array<float, 3> angular_velocity{0,0,0};// rad/s
};

// B_SYS_STATUS (id=25)
struct SysStatusMsg {
    Header header;
    uint8_t pwr_off = 0;                 // 1=10s后断电
    BChargeState charge_state = BChargeState::NONE;
    uint8_t battery_level = 0;           // 0-100
};

//====================
// A板内部消息/Web发送消息
//====================

// 按键状态
struct ButtonStatusEventMsg
{
    Header header;
    int  button_id; // 0-left 1-right
    int  button_stat; // 0=未按下，1=按下, 2=长按
};

struct VisualFeatureFrame
{
    Header header;
    int frame_index;
    std::vector<std::array<float, 5>> faces;
    std::array<float, 7> macro_expression;
    // Face heading angles in degrees; positive yaw => face on image right, positive pitch => face on image lower side
    float face_heading_yaw;
    float face_heading_pitch;
};

struct LLMEmotionDetResult
{
    Header header;
    std::string analysis_str;
    std::string analysis_id;
    float confidence;
    std::array<std::string, 7> emotions_labels;
    std::array<float, 7> emotions_probs;
};

struct LLMEmotionIntention
{
    Header header;
    std::string intention_name;
    float confidence;
    std::string reasoning;
};

// A板消息定义
struct VitalData
{
    Header header;
    int init_stat;
    int maxd;
    int presence_status;
    float conf;
    float hr_est;
    float rr_est;
    float rr_amp;
};

struct SystemStatInfo
{
    Header header;
    std::array<float, 3> cat_pad_values;
    float cat_trust_value;
};

struct VitalVisData
{
    Header header;
    float ppg_waveform[256];
    float breath_waveform[256];
};

// struct EyeballPosition
// {
//     int16_t eye_center_offset_x;
//     int16_t eye_center_offset_y;
// };

struct EyeballDispalyCommand
{
    Header header;
    int eyeball_id; // 0=左眼，1=右眼，2=双眼
    float blink_rate; // 眨眼频率，0-1 待定
    std::vector<std::string> file_path; // 如果需要切换：
                                        // 素材文件路径列表:left_eyelid, left_eye, left_highlight, 
                                        // right_eyelid, right_eye, right_highlight
                                        // 若不切换则留空
    bool enable_tracking; // 是否使能追踪
};

struct AudioPlayCommand
{
    Header header;
    std::string file_path;
    float speed;
    float volume;
    bool loop;
};

struct LedControlMsg
{
    Header header;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    float brightness;
};

// 舵机控制器类型枚举
enum class MotorControllerType : uint8_t {
    IDLE = 0,        // 保持当前位置（仍使能）
    SINUSOIDAL = 1,  // 正弦运动
    MOVE_TO = 2,     // 移动到目标位置
    // 未来可添加其他类型
};

// 单个舵机的控制器配置
struct MotorControllerConfig
{
    int motor_idx;                              // 舵机索引 (0-7)
    MotorControllerType controller_type;        // 控制器类型
    std::vector<float> parameters;              // 控制器参数（按参数名字典序排列）
    
    // 构造函数
    MotorControllerConfig() : motor_idx(0), controller_type(MotorControllerType::IDLE) {}
    
    MotorControllerConfig(int idx, MotorControllerType type, const std::vector<float>& params = {})
        : motor_idx(idx), controller_type(type), parameters(params) {}
};

// 组合动作的单个步骤配置
struct ActionGroupStep
{
    std::string name;                                     // 步骤名称（可选，便于日志）
    std::vector<MotorControllerConfig> motor_configs;     // 本步骤的舵机配置
    int32_t duration_ms{0};                               // 该步最大执行时长（0 表示未指定）
    int32_t idle_time_ms{0};                              // 该步完成后的空闲等待
    bool loop{true};                                      // 对正弦类动作是否循环
};

struct ActionGroupExecuteCommand
{
    Header header;
    std::string action_name;
    std::vector<MotorControllerConfig> motor_configs;  // 每个舵机的控制器配置（单步）
    float speed_scale;
    bool blocking;

    // 动作时间/循环元数据
    int32_t duration_ms{0};                            // 顶层时长（无序列时生效）
    int32_t idle_time_ms{0};                           // 顶层空闲时间
    bool loop{true};                                   // 顶层正弦是否循环

    // 组合动作序列（可选，存在时优先生效）
    std::vector<ActionGroupStep> steps;
    
    // 为了向后兼容，保留旧的parameters字段（已弃用）
    // 如果motor_configs为空，则使用parameters解析
    std::vector<float> parameters;
};

// ====================
// 音频 ADTS 数据流
// ====================


// 流开始/配置（便于订阅端建立解码器与缓存）
struct AdtsStreamControlMsg {
    Header header;
    // uint32_t stream_id = 0;                // 流唯一ID（同一设备内）
    bool is_start = true;               // true=开始，false=结束    
    //AudioCodec codec = AudioCodec::AAC_ADTS;
    uint32_t sample_rate = 48000;          // 采样率
    uint8_t channels = 1;                  // 声道数
    uint32_t bit_rate = 64000;             // 目标码率（可选）
    uint8_t aot = 2;                       // AAC 对象类型：2=LC，与 FDK AOT_AAC_LC 对齐
};

// 数据分片：每个 MQTT 包携带若干 ADTS 帧，或某一帧的一个分片
struct AdtsStreamDataMsg {
    Header header;
    // uint32_t stream_id = 0;                // 关联流ID
    uint32_t seq = 0;                       // 包序号（递增，便于乱序重排/丢包检测）
    uint64_t pts_ms = 0;                    // 此包首帧的展示时间戳（毫秒）
    uint16_t frame_count = 1;               // 负载中完整 ADTS 帧数量（按 ADTS 头计数；分片时可为 0）
    // uint16_t chunk_index = 0;               // 分片序号（未分片=0）
    // uint16_t chunk_count = 1;               // 分片总数（未分片=1）
    // bool contains_adts_header = true;       // 负载是否包含 ADTS 头（通常为 true）
    std::vector<uint8_t> payload;           // 原始 ADTS 字节（可含多帧或分片）
};

struct SoundLocalizationMsg {
    Header header;
    float azimuth_deg = 0.0f;    // 声源方位角，单位度
    float elevation_deg = 0.0f;  // 声源仰俯角，单位度
    float confidence = 0.0f;     // 置信度，范围0.0-1.0
    float loudness[4] = {0.0f};        // 声源响度，单位分贝 ,采用dBFS标准
};

}  // namespace mqttMsgs
} // namespace bionicCat



#endif // BIONIC_CAT_MQTT_MSG_HPP