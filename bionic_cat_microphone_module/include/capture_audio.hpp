#ifndef CAPTURE_AUDIO_HPP
#define CAPTURE_AUDIO_HPP

#include <cstdint>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <memory>

struct pcm; // tinyalsa 的前向声明

#include <fdk-aac/aacenc_lib.h>
#include "sound_localization.hpp"

namespace BionicCat {
namespace MicrophoneModule {

// 与未来 MQTT 消息对应的精简结构（不含 Header），便于直接填充 MQ 消息体
struct AdtsStreamControl {
    bool is_start{true};
    uint32_t sample_rate{16000};
    uint8_t channels{1};
    uint32_t bit_rate{64000};
    uint8_t aot{2}; // 2=LC
};

struct AdtsStreamData {
    uint32_t seq{0};
    uint64_t pts_ms{0};
    uint16_t frame_count{1};
    std::vector<uint8_t> payload; // ADTS 带头字节
};

// 适配声源定位 YAML 的音频配置（与 MicArrayConfig 的 audio 字段对应）
struct AudioConfig {
    uint32_t sample_rate{16000};
    uint32_t frame_size{1024}; // 用作 period_size
    int32_t max_delay_samples{64};
    uint32_t channels{1};
    float audio_gain{1.0f};
    int period_count{4};
};

struct sound_localization_result
{
    float azimuth{0.0f};
    float elevation{0.0f};
    float confidence{0.0f};
    float loudness[4]{0.0f};
};


// 只做设备采集的轻量封装（阻塞按 period 读取）
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    // 原始打开方法
    bool open(int card, int device, int rate, int channels,
              int period_size = 1024, int period_count = 4);

    // 新增：基于 AudioConfig 的多通道打开方法（用于声源定位配置）
    bool openMulti(int card, int device, const AudioConfig& cfg);

    void close();

    // 读取一个 period 的 PCM（S16LE），返回是否成功（交织型：frames*channels）
    bool readPeriod(std::vector<int16_t>& out);

    // 读取一个 period 的 PCM 并按通道拆分（去交织）
    // outPerChannel.size() 将被设置为 channels()，每个向量长度为 periodSize()
    bool readMultiPeriod(std::vector<std::vector<int16_t>>& outPerChannel);


    int rate() const { return rate_; }
    int channels() const { return channels_; }
    int periodSize() const { return period_size_; }

private:
    pcm* pcm_{nullptr};
    int rate_{16000};
    int channels_{1};
    int period_size_{1024};
    int period_count_{4};
    float gain_{1.0f}; // 新增：采集输出增益（用于声源定位）
};

class AACEncoder {
public:
    AACEncoder();
    ~AACEncoder();

    bool init(int sample_rate, int channels, int bitrate, int aot = 2);
    // 输入 PCM（S16LE，samples 为采样点个数=样本数×通道数），输出一帧 ADTS 字节（可能为空）
    bool encode(const int16_t* pcm_data, size_t samples, std::vector<uint8_t>& out_adts);
    void close();

    int frameSamplesPerCh() const { return input_samples_per_frame_; }

private:
    static void writeAdtsHeader(uint8_t* adts, int aac_length, int samplerate, int channels);

private:
    HANDLE_AACENCODER encoder_{nullptr};
    int input_samples_per_frame_{1024}; // per channel
    int samplerate_{16000};
    int channels_{1};
};

// 采集+编码+打包成 ADTS，提供回调接口（用于发布 MQTT 消息）
// 注意：无论采集通道数是多少，AAC 编码与 ADTS 打包固定为单通道（Mono）
class MicrophoneAdtsStreamer {
public:
    struct Config {
        int card{0};
        int device{0};
        uint32_t sample_rate{16000};
        uint8_t channels{1}; // 采集通道数；编码固定使用 1
        uint32_t bit_rate{64000};
        uint8_t aot{2};
        int period_size{1024};
        int period_count{4};
        // 新增：声源定位相关
        bool enable_localization{false};
        std::string localization_config_path{"/mnt/data/CV184X/bionic_cat/config/custom_3d_mic_config.yaml"}; // YAML 配置路径
    };

    using ControlCallback = std::function<void(const AdtsStreamControl&)>;
    using DataCallback = std::function<void(const AdtsStreamData&)>;
    using LocalizationCallback = std::function<void(const sound_localization_result&)>; // 新增

    MicrophoneAdtsStreamer();
    ~MicrophoneAdtsStreamer();

    bool start(const Config& cfg);
    void stop();

    void onControl(ControlCallback cb) { ctrl_cb_ = std::move(cb); }
    void onData(DataCallback cb) { data_cb_ = std::move(cb); }
    void onLocalization(LocalizationCallback cb) { loc_cb_ = std::move(cb); } // 新增

private:
    // 生产者：采集并推入队列
    void run();
    // 消费者：编码 ch0 并输出 ADTS
    void runEncode();
    // 消费者：声源定位（可选）
    void runLocalize();
    static uint64_t nowMs();

private:
    Config cfg_{};
    std::atomic<bool> running_{false};
    // 线程
    std::thread producer_;
    std::thread encoder_;
    std::thread localizer_thr_;
    // 双队列与同步
    using FramePtr = std::shared_ptr<std::vector<std::vector<int16_t>>>;
    std::deque<FramePtr> encode_queue_;
    std::deque<FramePtr> localize_queue_;
    std::mutex queue_mtx_;
    std::condition_variable queue_cv_;
    size_t max_queue_size_{32};

    AudioCapture cap_;
    AACEncoder enc_;
    ControlCallback ctrl_cb_{};
    DataCallback data_cb_{};
    LocalizationCallback loc_cb_{};

    // 声源定位实例与配置
    MicArrayConfig mic_cfg_{}; // 新增：定位用配置
    std::unique_ptr<MicArrayLocalizer> localizer_; // 新增
};

} // namespace MicrophoneModule
} // namespace BionicCat

#endif // CAPTURE_AUDIO_HPP