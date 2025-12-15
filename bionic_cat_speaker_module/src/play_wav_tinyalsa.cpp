#include "play_wav_tinyalsa.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <future>
#include <memory>
#include <pthread.h>
#include <sched.h>

// tinyalsa 头文件
#if __has_include(<tinyalsa/asoundlib.h>)
  #include <tinyalsa/asoundlib.h>
#elif __has_include(<asoundlib.h>)
  #include <asoundlib.h>
#else
  #error "tinyalsa asoundlib.h not found"
#endif

WavPlayer::WavPlayer() {}

WavPlayer::~WavPlayer() {
    close();
}

// ---------------------- 核心控制逻辑 ----------------------

void WavPlayer::ensureThreadStarted() {
    if (thread_started_) return;
    terminate_.store(false);
    stop_flag_.store(false);
    playing_.store(false);
    th_ = std::thread(&WavPlayer::playbackThread, this);

    // // 设置线程实时优先级
    // struct sched_param sch; sch.sched_priority = 20;
    // int ret = pthread_setschedparam(th_.native_handle(), SCHED_FIFO, &sch);
    // if (ret != 0) {
    //     std::cerr << "[WavPlayer] pthread_setschedparam failed: " << std::strerror(ret) << std::endl;
    // }
    thread_started_ = true;
}

void WavPlayer::stop() {
    // 停止当前播放，但不回收线程
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_flag_.store(true);
    }
    cv_.notify_all();

    // 等待播放线程把 playing_ 置为 false（非强制 join）
    // 保留文件句柄关闭
    if (fp_) { std::fclose(fp_); fp_ = nullptr; }
    playing_.store(false);
}

void WavPlayer::close() {
    // 终止常驻线程并关闭设备
    {
        std::lock_guard<std::mutex> lk(mtx_);
        terminate_.store(true);
        stop_flag_.store(true);
    }
    cv_.notify_all();

    if (th_.joinable()) th_.join();
    thread_started_ = false;

    closeDevice();
}

void WavPlayer::stopPlaybackThread() {
    // 保持与 close 行为一致，避免死锁
    {
        std::lock_guard<std::mutex> lk(mtx_);
        terminate_.store(true);
        stop_flag_.store(true);
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
    thread_started_ = false;

    playing_.store(false);

    if (fp_) {
        std::fclose(fp_);
        fp_ = nullptr;
    }
}

void WavPlayer::closeDevice() {
    if (pcm_) {
        pcm_close(pcm_);
        pcm_ = nullptr;
    }
    if (cfg_) {
        delete cfg_;
        cfg_ = nullptr;
    }
    device_opened_ = false;
    last_channels_ = 0;
    last_rate_ = 0;
    last_bits_ = 0;
    last_card_ = -1;
    last_device_ = -1;
}

bool WavPlayer::load(int card, int device) {
   // 不销毁线程，仅准备资源
    if (file_path.empty()) {
        std::cerr << "[WavPlayer] Error: file_path is empty" << std::endl;
        return false;
    }

    fp_ = std::fopen(file_path.c_str(), "rb");
    if (!fp_) {
        std::perror("[WavPlayer] fopen failed");
        return false;
    }

    if (!readHeader()) {
        std::cerr << "[WavPlayer] Error: Invalid WAV header" << std::endl;
        std::fclose(fp_);
        fp_ = nullptr;
        return false;
    }

    if (!openPcm(card, device)) {
        std::fclose(fp_);
        fp_ = nullptr;
        return false;
    }

    return true;
}

bool WavPlayer::play() {
    if (!fp_ || !pcm_) {
        std::cerr << "[WavPlayer] Error: Not loaded" << std::endl;
        return false;
    }

    ensureThreadStarted();

    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_flag_.store(false);
        play_request_.store(true);
    }
    cv_.notify_all();
    return true;
}

// ---------------------- 内部实现细节 ----------------------

bool WavPlayer::readHeader() {
    if (!fp_) return false;
    if (fseek(fp_, 0, SEEK_SET) != 0) return false;
    if (fread(&header_, sizeof(header_), 1, fp_) != 1) return false;
    if (std::strncmp(header_.riff, "RIFF", 4) != 0 || std::strncmp(header_.wave, "WAVE", 4) != 0) {
        return false;
    }
    data_start_pos_ = ftell(fp_);
    return true;
}

bool WavPlayer::openPcm(int card, int device) {
    // 尝试复用
    if (device_opened_ &&
        last_card_ == card &&
        last_device_ == device &&
        last_channels_ == header_.num_channels &&
        last_rate_ == header_.sample_rate &&
        last_bits_ == header_.bits_per_sample) {
        return true; 
    }

    closeDevice();

    // 分配并填充 cfg_
    cfg_ = new pcm_config{};
    std::memset(cfg_, 0, sizeof(pcm_config));
    cfg_->channels = header_.num_channels;
    cfg_->rate = header_.sample_rate;
    // 缓冲区设置：较大以防断流
    cfg_->period_size = 1024; 
    cfg_->period_count = 4;
    cfg_->start_threshold = cfg_->period_size;
    cfg_->stop_threshold = cfg_->period_size * cfg_->period_count;
    cfg_->silence_threshold = 0;
    cfg_->avail_min = 1;

    switch (header_.bits_per_sample) {
        case 8:  cfg_->format = PCM_FORMAT_S8; break;
        case 16: cfg_->format = PCM_FORMAT_S16_LE; break;
        case 24: cfg_->format = PCM_FORMAT_S24_LE; break;
        case 32: cfg_->format = PCM_FORMAT_S32_LE; break;
        default:
            std::cerr << "[WavPlayer] Unsupported bit depth: " << header_.bits_per_sample << std::endl;
            delete cfg_; cfg_ = nullptr;
            return false;
    }

    std::cout << "[WavPlayer] Opening PCM card=" << card << " device=" << device 
              << " rate=" << cfg_->rate << " ch=" << cfg_->channels << "..." << std::endl;

    // 使用线程在后台执行 pcm_open，防止主线程被长时间阻塞
    // 传递 cfg 的副本到后台线程，避免 cfg_ 在此期间被释放导致悬空
    auto cfg_copy = std::make_shared<pcm_config>();
    std::memcpy(cfg_copy.get(), cfg_, sizeof(pcm_config));

    const unsigned int flags = PCM_OUT | PCM_NORESTART | PCM_MONOTONIC;

    auto t0 = std::chrono::steady_clock::now();

    // 使用 promise/future + thread，这样可以在线程中设置优先级并可控地等待
    std::promise<struct pcm*> prom;
    std::future<struct pcm*> fut = prom.get_future();

    std::thread open_thread([card, device, flags, cfg_copy, p = std::move(prom)]() mutable {
        // 提升此打开线程为实时优先级，若权限不足会返回错误但仍继续尝试打开
        struct sched_param sch;
        sch.sched_priority = 20; // 可调整
        int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch);
        if (ret != 0) {
            std::cerr << "[WavPlayer] open_thread: pthread_setschedparam failed: " << std::strerror(ret) << std::endl;
        }

        struct pcm* result = pcm_open(card, device, flags, cfg_copy.get());
        try {
            p.set_value(result);
        } catch (...) {
            if (result) pcm_close(result);
        }
    });

    // 等待最大超时时间
    const auto timeout = std::chrono::seconds(30);
    if (fut.wait_for(timeout) != std::future_status::ready) {
        std::cerr << "[WavPlayer] pcm_open timeout after " << std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count() << "ms" << std::endl;
        // 后台线程可能仍在运行，detach 让它在后台完成；清理本次分配并返回失败
        open_thread.detach();
        delete cfg_; cfg_ = nullptr;
        return false;
    }

    pcm_ = fut.get();
    // 确保线程已结束
    if (open_thread.joinable()) open_thread.join();

    auto t1 = std::chrono::steady_clock::now();

    if (!pcm_ || !pcm_is_ready(pcm_)) {
        std::cerr << "[WavPlayer] PCM open failed: " << (pcm_ ? pcm_get_error(pcm_) : "unknown") << std::endl;
        if (pcm_) {
            pcm_close(pcm_);
            pcm_ = nullptr;
        }
        delete cfg_; cfg_ = nullptr;
        return false;
    }

    std::cout << "[WavPlayer] pcm_open ok, elapsed="
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << "ms" << std::endl;

    device_opened_ = true;
    last_card_ = card;
    last_device_ = device;
    last_channels_ = header_.num_channels;
    last_rate_ = header_.sample_rate;
    last_bits_ = header_.bits_per_sample;
    return true;
}

void WavPlayer::playbackThread() {
    // 尝试在播放线程内部设置实时优先级（有时这比外部设置更可靠）
    struct sched_param sch; sch.sched_priority = 20;
    int pr = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch);
    if (pr != 0) {
        std::cerr << "[WavPlayer] playbackThread: pthread_setschedparam failed: " << std::strerror(pr) << std::endl;
    }

    while (true) {
        // 等待播放请求或终止
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&]{ return play_request_.load() || terminate_.load(); });
        }
        if (terminate_.load()) break;

        // 开始一次播放
        playing_.store(true);
        play_request_.store(false);

        const size_t rawBlock = 4096;
        std::vector<uint8_t> inBuf(rawBlock);
        std::vector<uint8_t> procBuf;

        if (!fp_) { playing_.store(false); continue; }
        if (fseek(fp_, data_start_pos_, SEEK_SET) != 0) { playing_.store(false); continue; }

        if (pcm_prepare(pcm_) != 0) {
            std::cerr << "[WavPlayer] pcm_prepare failed" << std::endl;
        }

        bool firstWrite = true;
        auto t_start = std::chrono::steady_clock::now();

        while (!stop_flag_.load()) {
            size_t n = fread(inBuf.data(), 1, rawBlock, fp_);
            if (n == 0) {
                if (loop.load()) { fseek(fp_, data_start_pos_, SEEK_SET); continue; }
                std::fill(inBuf.begin(), inBuf.end(), 0); n = rawBlock;
            }

            applyVolumeAndSpeed(inBuf.data(), n, procBuf);
            if (procBuf.empty()) { continue; }

            uint8_t* ptr = procBuf.data();
            size_t remaining = procBuf.size();

            while (remaining > 0 && !stop_flag_.load()) {
                int r = pcm_write(pcm_, ptr, remaining);
                if (r == 0) {
                    ptr += remaining; remaining = 0;
                    if (firstWrite) {
                        firstWrite = false;
                        auto t_fw = std::chrono::steady_clock::now();
                        std::cout << "[WavPlayer] First audio buffer written, latency="
                                  << std::chrono::duration_cast<std::chrono::milliseconds>(t_fw - t_start).count() << "ms" << std::endl;
                    }
                } else if (r == -EPIPE || r == -32) {
                    if (stop_flag_.load()) break;
                    r = pcm_prepare(pcm_);
                    if (r != 0) { std::cerr << "[WavPlayer] Recovery failed: " << r << std::endl; break; }
                } else {
                    if (stop_flag_.load()) break;
                    std::cerr << "[WavPlayer] pcm_write error: " << r << " (" << pcm_get_error(pcm_) << ")" << std::endl;
                    break;
                }
            }
        }
        // 播放完成或被停止
        playing_.store(false);
        stop_flag_.store(true);
    }
}

void WavPlayer::applyVolumeAndSpeed(const uint8_t* in, size_t inBytes, std::vector<uint8_t>& out) {
    float curSpeed = speed.load();
    float curVol = volume.load();

    const int channels = header_.num_channels;
    const int bps = header_.bits_per_sample / 8;
    if (bps <= 0) return;

    size_t frameCount = inBytes / (channels * bps);
    if (frameCount == 0) { out.clear(); return; }

    size_t targetFrames;
    if (curSpeed > 0.999f && curSpeed < 1.001f) {
        targetFrames = frameCount;
    } else if (curSpeed > 1.0f) {
        targetFrames = static_cast<size_t>(frameCount / curSpeed);
    } else {
        targetFrames = static_cast<size_t>(frameCount / curSpeed);
    }

    out.resize(targetFrames * channels * bps);

    auto copyFrame = [&](size_t srcIndex, size_t dstIndex) {
        if (srcIndex >= frameCount) srcIndex = frameCount - 1;
        const uint8_t* src = in + srcIndex * channels * bps;
        uint8_t* dst = out.data() + dstIndex * channels * bps;
        std::memcpy(dst, src, channels * bps);
    };

    double step = curSpeed;
    double pos = 0.0;
    for (size_t d = 0; d < targetFrames; ++d) {
        copyFrame(static_cast<size_t>(pos), d);
        pos += step;
    }

    if (std::abs(curVol - 1.0f) > 0.001f) {
        size_t samples = out.size() / bps;
        if (bps == 1) { // 8-bit
            for (size_t i = 0; i < samples; ++i) {
                int v = static_cast<int>(static_cast<int8_t>(out[i]));
                v = static_cast<int>(v * curVol);
                v = std::clamp(v, -128, 127);
                out[i] = static_cast<uint8_t>(static_cast<int8_t>(v));
            }
        } else if (bps == 2) { // 16-bit
            int16_t* p = reinterpret_cast<int16_t*>(out.data());
            for (size_t i = 0; i < samples; ++i) {
                int v = static_cast<int>(p[i]);
                v = static_cast<int>(v * curVol);
                v = std::clamp(v, -32768, 32767);
                p[i] = static_cast<int16_t>(v);
            }
        } else if (bps == 3) { // 24-bit
             for (size_t i = 0; i < samples; ++i) {
                uint8_t* s = out.data() + i * 3;
                int v = (static_cast<int>(s[0]) | (static_cast<int>(s[1]) << 8) | (static_cast<int>(s[2]) << 16));
                if (v & 0x800000) v |= ~0xFFFFFF; // sign extend
                v = static_cast<int>(v * curVol);
                v = std::clamp(v, -8388608, 8388607);
                s[0] = static_cast<uint8_t>(v & 0xFF);
                s[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
                s[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
            }
        } else if (bps == 4) { // 32-bit
            int32_t* p = reinterpret_cast<int32_t*>(out.data());
            for (size_t i = 0; i < samples; ++i) {
                long long v = static_cast<long long>(p[i]);
                v = static_cast<long long>(v * curVol);
                v = std::clamp<long long>(v, -2147483648LL, 2147483647LL);
                p[i] = static_cast<int32_t>(v);
            }
        }
    }
}