#include "capture_audio.hpp"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <thread>
#include <array>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <ostream>
// 仅在实现中包含 tinyalsa 头，兼容不同安装路径
#if __has_include(<tinyalsa/asoundlib.h>)
  #include <tinyalsa/asoundlib.h>
#elif __has_include(<asoundlib.h>)
  #include <asoundlib.h>
#else
  #error "tinyalsa asoundlib.h not found"
#endif

// 集成声源定位
#include "sound_localization.hpp"

namespace BionicCat {
namespace MicrophoneModule {

// -------- AudioCapture --------
AudioCapture::AudioCapture() = default;
AudioCapture::~AudioCapture() { close(); }

bool AudioCapture::open(int card, int device, int rate, int channels,
                        int period_size, int period_count) {
    if (pcm_) close();

    struct pcm_config cfg{};
    cfg.channels = static_cast<unsigned int>(channels);
    cfg.rate = static_cast<unsigned int>(rate);
    cfg.period_size = static_cast<unsigned int>(period_size);
    cfg.period_count = static_cast<unsigned int>(period_count);
    cfg.format = PCM_FORMAT_S16_LE;

    pcm* handle = pcm_open(card, device, PCM_IN, &cfg);
    if (!handle || !pcm_is_ready(handle)) {
        std::fprintf(stderr, "PCM open failed: %s\n", handle ? pcm_get_error(handle) : "");
        if (handle) pcm_close(handle);
        return false;
    }

    pcm_ = handle;
    rate_ = rate;
    channels_ = channels;
    period_size_ = period_size;
    period_count_ = period_count;
    gain_ = 1.0f;
    return true;
}

// 新增：基于 AudioConfig 的打开方法
bool AudioCapture::openMulti(int card, int device, const AudioConfig& cfg) {
    if (pcm_) close();
    struct pcm_config pc{};
    pc.channels = static_cast<unsigned int>(cfg.channels);
    pc.rate = static_cast<unsigned int>(cfg.sample_rate);
    pc.period_size = static_cast<unsigned int>(cfg.frame_size); // 对齐声源定位 frame_size
    pc.period_count = static_cast<unsigned int>(cfg.period_count);
    pc.format = PCM_FORMAT_S16_LE;
    pcm* handle = pcm_open(card, device, PCM_IN, &pc);
    if (!handle || !pcm_is_ready(handle)) {
        std::fprintf(stderr, "PCM open failed: %s\n", handle ? pcm_get_error(handle) : "");
        if (handle) pcm_close(handle);
        return false;
    }
    pcm_ = handle;
    rate_ = static_cast<int>(cfg.sample_rate);
    channels_ = static_cast<int>(cfg.channels);
    period_size_ = static_cast<int>(pc.period_size);
    period_count_ = static_cast<int>(pc.period_count);
    gain_ = cfg.audio_gain; // 设置增益
    return true;
}

void AudioCapture::close() {
    if (pcm_) {
        pcm_close(pcm_);
        pcm_ = nullptr;
    }
}

bool AudioCapture::readPeriod(std::vector<int16_t>& out) {
    if (!pcm_) return false;
    const int bytes = period_size_ * channels_ * 2; // s16le
    out.resize(static_cast<size_t>(period_size_ * channels_));
    if (pcm_read(pcm_, out.data(), bytes) != 0) {
        std::fprintf(stderr, "pcm_read error: %s\n", pcm_get_error(pcm_));
        return false;
    }
    // 应用增益并饱和
    if (gain_ != 1.0f) {
        for (size_t i = 0; i < out.size(); ++i) {
            int32_t v = static_cast<int32_t>(std::lround(out[i] * gain_));
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            out[i] = static_cast<int16_t>(v);
        }
    }
    return true;
}

// 不复用交织读取：直接从设备读入并去交织
bool AudioCapture::readMultiPeriod(std::vector<std::vector<int16_t>>& outPerChannel) {
    if (!pcm_) return false;
    const int frames = period_size_;
    const int ch = channels_;
    const int samples = frames * ch;
    const int bytes = samples * 2; // s16le

    std::vector<int16_t> buf;
    buf.resize(static_cast<size_t>(samples));
    if (pcm_read(pcm_, buf.data(), bytes) != 0) {
        std::fprintf(stderr, "pcm_read error: %s\n", pcm_get_error(pcm_));
        return false;
    }

    outPerChannel.assign(static_cast<size_t>(ch), std::vector<int16_t>(static_cast<size_t>(frames)));
    for (int f = 0; f < frames; ++f) {
        const int base = f * ch;
        for (int c = 0; c < ch; ++c) {
            int32_t v = static_cast<int32_t>(buf[static_cast<size_t>(base + c)]);
            if (gain_ != 1.0f) {
                v = static_cast<int32_t>(std::lround(v * gain_));
            }
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            outPerChannel[c][static_cast<size_t>(f)] = static_cast<int16_t>(v);
        }
    }
    return true;
}

// -------- AACEncoder --------
AACEncoder::AACEncoder() = default;
AACEncoder::~AACEncoder() { close(); }

bool AACEncoder::init(int sample_rate, int channels, int bitrate, int aot) {
    close();

    AACENC_ERROR err;
    if ((err = aacEncOpen(&encoder_, 0, channels)) != AACENC_OK) {
        std::fprintf(stderr, "aacEncOpen(): %d\n", err);
        return false;
    }

    aacEncoder_SetParam(encoder_, AACENC_AOT, aot == 0 ? 2 : aot);
    aacEncoder_SetParam(encoder_, AACENC_SAMPLERATE, sample_rate);
    aacEncoder_SetParam(encoder_, AACENC_CHANNELMODE, channels == 1 ? MODE_1 : MODE_2);
    aacEncoder_SetParam(encoder_, AACENC_BITRATE, bitrate);
    // 使用 RAW 传输流，由我们手动添加 ADTS 头，避免重复 ADTS 导致解码错误
    aacEncoder_SetParam(encoder_, AACENC_TRANSMUX, TT_MP4_RAW);

    if ((err = aacEncEncode(encoder_, nullptr, nullptr, nullptr, nullptr)) != AACENC_OK) {
        std::fprintf(stderr, "aacEncEncode(start): %d\n", err);
        close();
        return false;
    }

    AACENC_InfoStruct info{};
    if (aacEncInfo(encoder_, &info) != AACENC_OK) {
        std::fprintf(stderr, "aacEncInfo failed\n");
        close();
        return false;
    }
    // FDK 默认 1024 samples / ch / frame for LC
    input_samples_per_frame_ = info.frameLength;
    samplerate_ = sample_rate;
    channels_ = channels;
    return true;
}

static int sfIndexFromRate(int samplerate) {
    switch (samplerate) {
        case 96000: return 0;
        case 88200: return 1;
        case 64000: return 2;
        case 48000: return 3;
        case 44100: return 4;
        case 32000: return 5;
        case 24000: return 6;
        case 22050: return 7;
        case 16000: return 8;
        case 12000: return 9;
        case 11025: return 10;
        case 8000:  return 11;
        default:    return 3;
    }
}

void AACEncoder::writeAdtsHeader(uint8_t* adts, int aac_length, int samplerate, int channels) {
    const int sf_index = sfIndexFromRate(samplerate);
    const int adts_len = aac_length + 7;

    // ADTS Profile 固定为 AAC LC (1)，确保与编码配置匹配
    const int profile = 1; // AAC LC

    adts[0] = 0xFF;
    adts[1] = 0xF1;               // MPEG-4, layer=0, protection_absent=1
    adts[2] = (profile << 6) | (sf_index << 2) | ((channels & 0x4) >> 2);
    adts[3] = ((channels & 0x3) << 6) | ((adts_len >> 11) & 0x03);
    adts[4] = (adts_len >> 3) & 0xFF;
    adts[5] = ((adts_len & 0x7) << 5) | 0x1F; // buffer fullness 0x7FF (VBR)
    adts[6] = 0xFC; // number_of_raw_data_blocks_in_frame = 0
}

bool AACEncoder::encode(const int16_t* pcm_data, size_t samples, std::vector<uint8_t>& out_adts) {
    if (!encoder_) return false;
    // 准备输入缓冲
    AACENC_BufDesc in_buf{}; AACENC_BufDesc out_buf{};
    AACENC_InArgs in_args{}; AACENC_OutArgs out_args{};

    void* in_ptr = const_cast<int16_t*>(pcm_data);
    int in_size = static_cast<int>(samples * sizeof(int16_t));
    int in_elem = sizeof(int16_t);

    in_buf.numBufs = 1;
    in_buf.bufs = &in_ptr;
    int in_id = IN_AUDIO_DATA; // 需要稳定地址
    in_buf.bufferIdentifiers = &in_id;
    in_buf.bufSizes = &in_size;
    in_buf.bufElSizes = &in_elem;

    uint8_t aac_buf[4096];
    void* out_ptr = aac_buf;
    int out_size = static_cast<int>(sizeof(aac_buf));
    int out_elem = 1;

    out_buf.numBufs = 1;
    out_buf.bufs = &out_ptr;
    int out_id = OUT_BITSTREAM_DATA;
    out_buf.bufferIdentifiers = &out_id;
    out_buf.bufSizes = &out_size;
    out_buf.bufElSizes = &out_elem;

    // FDK 需要的是每通道样本数
    in_args.numInSamples = static_cast<int>(samples);

    const AACENC_ERROR err = aacEncEncode(encoder_, &in_buf, &out_buf, &in_args, &out_args);
    if (err != AACENC_OK) {
        std::fprintf(stderr, "aacEncEncode(): %d\n", err);
        return false;
    }

    out_adts.clear();
    if (out_args.numOutBytes > 0) {
        uint8_t adts[7];
        writeAdtsHeader(adts, out_args.numOutBytes, samplerate_, channels_);
        out_adts.reserve(7 + out_args.numOutBytes);
        out_adts.insert(out_adts.end(), adts, adts + 7);
        out_adts.insert(out_adts.end(), aac_buf, aac_buf + out_args.numOutBytes);
        return true;
    }
    return true; // 仅表示调用成功，可能没有输出（编码器内部缓冲）
}

void AACEncoder::close() {
    if (encoder_) {
        aacEncClose(&encoder_);
        encoder_ = nullptr;
    }
}

// -------- MicrophoneAdtsStreamer --------
MicrophoneAdtsStreamer::MicrophoneAdtsStreamer() = default;
MicrophoneAdtsStreamer::~MicrophoneAdtsStreamer() { stop(); }

bool MicrophoneAdtsStreamer::start(const Config& cfg) {
    stop();
    cfg_ = cfg;

    // 配置采集（若启用定位则读取 YAML 配置）
    AudioConfig acfg{};
    if (cfg.enable_localization) {
        mic_cfg_ = MicArrayLocalizer::loadConfig(cfg.localization_config_path);
        localizer_ = std::make_unique<MicArrayLocalizer>(mic_cfg_);
        acfg.sample_rate = mic_cfg_.sample_rate;
        acfg.frame_size = mic_cfg_.frame_size;
        acfg.channels = mic_cfg_.channels;
        acfg.audio_gain = mic_cfg_.audio_gain;
        acfg.period_count = cfg.period_count;
 
    } else {
        acfg.sample_rate = cfg.sample_rate;
        acfg.frame_size = cfg.period_size;
        acfg.channels = cfg.channels;
        acfg.audio_gain = 1.0f;
        acfg.period_count = cfg.period_count;
    }
    std::cout << "[MicrophoneAdtsStreamer] Loaded localization config: "
            << "sr=" << acfg.sample_rate
            << ", fs=" << acfg.frame_size
            << ", ch=" << acfg.channels
            << ", gain=" << acfg.audio_gain
            << std::endl;

    if (!cap_.openMulti(cfg.card, cfg.device, acfg)) {
        return false;
    }

    // 编码固定单通道
    if (!enc_.init(cfg.sample_rate, /*channels*/1, cfg.bit_rate, cfg.aot)) {
        cap_.close();
        return false;
    }

    running_.store(true);
    // 启动生产者与两个消费者线程
    producer_ = std::thread(&MicrophoneAdtsStreamer::run, this);
    encoder_  = std::thread(&MicrophoneAdtsStreamer::runEncode, this);
    if (cfg.enable_localization && localizer_) {
        std::cout << "[MicrophoneAdtsStreamer] Starting localization thread." << std::endl;
        localizer_thr_ = std::thread(&MicrophoneAdtsStreamer::runLocalize, this);
    }

    // 发送控制开始
    if (ctrl_cb_) {
        AdtsStreamControl c{};
        c.is_start = true;
        c.sample_rate = cfg.sample_rate;
        c.channels = 1; // 固定单声道编码
        c.bit_rate = cfg.bit_rate;
        c.aot = cfg.aot;
        ctrl_cb_(c);
    }
    return true;
}

void MicrophoneAdtsStreamer::stop() {
    if (!running_.exchange(false)) {
        // 若未运行，仍需 join 可能存在的线程
        if (producer_.joinable()) producer_.join();
        if (encoder_.joinable()) encoder_.join();
        if (localizer_thr_.joinable()) localizer_thr_.join();
        return;
    }

    // 唤醒等待队列的消费者
    {
        std::lock_guard<std::mutex> lk(queue_mtx_);
        queue_cv_.notify_all();
    }

    if (producer_.joinable()) producer_.join();
    if (encoder_.joinable()) encoder_.join();
    if (localizer_thr_.joinable()) localizer_thr_.join();

    cap_.close();
    enc_.close();

    if (ctrl_cb_) {
        AdtsStreamControl c{};
        c.is_start = false;
        c.sample_rate = cfg_.sample_rate;
        c.channels = cfg_.channels;
        c.bit_rate = cfg_.bit_rate;
        c.aot = cfg_.aot;
        ctrl_cb_(c);
    }
}

uint64_t MicrophoneAdtsStreamer::nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// 生产者：采集并推入队列
void MicrophoneAdtsStreamer::run() {
    while (running_.load()) {
        std::vector<std::vector<int16_t>> period_ch;
        if (!cap_.readMultiPeriod(period_ch)) {
            break;
        }
        if (period_ch.empty()) continue;
        // 为同一帧创建共享指针，分别推入两个队列
        FramePtr frame = std::make_shared<std::vector<std::vector<int16_t>>>(std::move(period_ch));
        {
            std::unique_lock<std::mutex> lk(queue_mtx_);
            if (encode_queue_.size() >= max_queue_size_) {
                encode_queue_.pop_front();
            }
            if (localize_queue_.size() >= max_queue_size_) {
                localize_queue_.pop_front();
            }
            encode_queue_.push_back(frame);
            localize_queue_.push_back(frame);
        }
        queue_cv_.notify_all();
    }
}

// 消费者：编码 ch0
void MicrophoneAdtsStreamer::runEncode() {
    const int frame_samples_per_ch = enc_.frameSamplesPerCh();
    const int frame_samples_total_mono = frame_samples_per_ch * 1; // 单通道编码

    std::vector<int16_t> mono_cache;
    mono_cache.reserve(static_cast<size_t>(frame_samples_total_mono * 2));

    uint32_t seq = 0;
    uint64_t pts = nowMs();

    while (running_.load()) {
        FramePtr frame;
        {
            std::unique_lock<std::mutex> lk(queue_mtx_);
            queue_cv_.wait(lk, [&]{ return !running_.load() || !encode_queue_.empty(); });
            if (!running_.load() && encode_queue_.empty()) break;
            frame = encode_queue_.front();
            encode_queue_.pop_front();
        }
        if (!frame || frame->empty()) continue;
        const size_t frames = static_cast<size_t>(cap_.periodSize());

        const std::vector<int16_t>& ch0 = (*frame)[0];
        mono_cache.insert(mono_cache.end(), ch0.begin(), ch0.end());

        while (mono_cache.size() >= static_cast<size_t>(frame_samples_total_mono)) {
            std::vector<uint8_t> adts;
            const bool ok = enc_.encode(mono_cache.data(), frame_samples_total_mono, adts);
            if (!ok) {
                running_.store(false);
                break;
            }
            mono_cache.erase(mono_cache.begin(), mono_cache.begin() + frame_samples_total_mono);

            if (!adts.empty() && data_cb_) {
                AdtsStreamData d{};
                d.seq = seq++;
                d.pts_ms = pts;
                d.frame_count = 1;
                d.payload = std::move(adts);
                data_cb_(d);
            }
            pts += static_cast<uint64_t>(1000.0 * frame_samples_per_ch / static_cast<double>(cfg_.sample_rate));
        }
    }
}

// 消费者：声源定位
void MicrophoneAdtsStreamer::runLocalize() {
    while (running_.load()) {
        FramePtr frame;
        {
            std::unique_lock<std::mutex> lk(queue_mtx_);
            queue_cv_.wait(lk, [&]{ return !running_.load() || !localize_queue_.empty(); });
            if (!running_.load() && localize_queue_.empty()) break;
            frame = localize_queue_.front();
            localize_queue_.pop_front();
        }
       // std::cout << "[MicrophoneAdtsStreamer] Localize thread processing frame." << std::endl;
        if (!frame || frame->size() < 4 || !localizer_ || !loc_cb_) {
            continue;
        }
        //std::cout << "[MicrophoneAdtsStreamer] Localize thread got valid frame." << std::endl;
        const size_t frames = static_cast<size_t>(cap_.periodSize());

        std::array<std::vector<float>, 4> ch_float;
        for (int c = 0; c < 4; ++c) {
            ch_float[c].resize(frames);
            const std::vector<int16_t>& src = (*frame)[c];
            for (size_t i = 0; i < frames; ++i) {
                ch_float[c][i] = static_cast<float>(src[i]) / 32768.0f;
            }
        }

        float azimuth_deg = 0.0f;
        float elevation_deg = 0.0f;
        float confidence = 0.0f;
        //std::cout << "[MicrophoneAdtsStreamer] ch_float: " << ch_float[0][10] << std::endl;
        const bool ok_loc = localizer_->localize(ch_float, static_cast<uint32_t>(frames), azimuth_deg, elevation_deg, confidence);
        //std::cout << "[MicrophoneAdtsStreamer] Localization computation done. Success: " << (ok_loc ? "Yes" : "No") << std::endl;
        if (ok_loc) {
            double db_ch[4] = {0,0,0,0};
            localizer_->calc4chSeparateDb(ch_float, db_ch);
            float loudness[4] = {
                static_cast<float>(db_ch[0]),
                static_cast<float>(db_ch[1]),
                static_cast<float>(db_ch[2]),
                static_cast<float>(db_ch[3])
            };

            sound_localization_result m{};
            m.azimuth = azimuth_deg;
            m.elevation = elevation_deg;
            m.confidence = confidence;
            m.loudness[0] = loudness[0];
            m.loudness[1] = loudness[1];
            m.loudness[2] = loudness[2];
            m.loudness[3] = loudness[3];
            loc_cb_(m);
        }
    }
}

} // namespace MicrophoneModule
} // namespace BionicCat