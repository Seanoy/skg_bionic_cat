#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>
#include <mutex>
#include <condition_variable>

// 前向声明，避免头文件依赖
struct pcm;
struct pcm_config;

// 简单的 WAV 头结构
struct WavHeader {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
};

class WavPlayer {
public:
    WavPlayer();
    ~WavPlayer();

    // 属性配置
    std::string file_path;
    std::atomic<float> speed{1.0f};  // 支持原子操作，便于运行时调整（虽目前主逻辑未动态调）
    std::atomic<float> volume{1.0f};
    std::atomic<bool> loop{false};

    // 加载文件并准备设备 (如果参数匹配则复用设备)
    bool load(int card = 0, int device = 0);

    // 开始播放 (异步)
    bool play();

    // 停止播放 (但不关闭音频设备，以便复用)
    void stop();

    // 完全关闭 (停止播放并关闭音频设备)
    void close();

    bool isPlaying() const { return playing_.load(); }

private:
    void playbackThread();
    bool openPcm(int card, int device);
    bool readHeader();
    void applyVolumeAndSpeed(const uint8_t* in, size_t inBytes, std::vector<uint8_t>& out);
    
    // 内部控制
    void stopPlaybackThread(); // 仅停止线程（改为仅供析构调用）
    void closeDevice();        // 仅关闭设备
    void ensureThreadStarted();

    FILE* fp_ = nullptr;
    long data_start_pos_ = 0;
    WavHeader header_{};

    struct pcm* pcm_ = nullptr;
    struct pcm_config* cfg_ = nullptr;

    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> playing_{false};
    std::thread th_;

    // 设备复用状态记录
    bool device_opened_ = false;
    uint16_t last_channels_ = 0;
    uint32_t last_rate_ = 0;
    uint16_t last_bits_ = 0;
    int last_card_ = -1;
    int last_device_ = -1;

    // 常驻线程控制
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> play_request_{false};
    std::atomic<bool> terminate_{false};
    bool thread_started_ = false;
};