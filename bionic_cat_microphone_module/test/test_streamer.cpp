#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "capture_audio.hpp"

using namespace BionicCat::MicrophoneModule;

static std::atomic<bool> g_stop{false};
static void on_signal(int) { g_stop.store(true); }

static void usage(const char* prog) {
    std::cout << "Usage: " << prog
              << " -o <out.aac> [-d <card=0>] [-D <device=0>] [-t <secs=5>]"
              << std::endl;
}

int main(int argc, char** argv) {
    int card = 0, device = 0, secs = 5;
    std::string outPath;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-o" || a == "--out") && i + 1 < argc) {
            outPath = argv[++i];
        } else if ((a == "-d" || a == "--card") && i + 1 < argc) {
            card = std::stoi(argv[++i]);
        } else if ((a == "-D" || a == "--device") && i + 1 < argc) {
            device = std::stoi(argv[++i]);
        } else if ((a == "-t" || a == "--time") && i + 1 < argc) {
            secs = std::stoi(argv[++i]);
        } else if (a == "-h" || a == "--help") {
            usage(argv[0]);
            return 0;
        }
    }

    if (outPath.empty()) {
        usage(argv[0]);
        return 1;
    }

    std::ofstream ofs(outPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "Failed to open output file: " << outPath << std::endl;
        return 2;
    }

    std::signal(SIGINT, on_signal);

    MicrophoneAdtsStreamer::Config cfg;
    cfg.card = card;
    cfg.device = device;
    cfg.sample_rate = 48000; // 可根据需要调整
    cfg.channels = 1;
    cfg.bit_rate = 64000;
    cfg.aot = 2;
    cfg.period_size = 1024;
    cfg.period_count = 4;

    MicrophoneAdtsStreamer streamer;

    // 控制消息回调（可对接到你的 MQTT 发布逻辑）
    streamer.onControl([&](const AdtsStreamControl& c){
        std::cout << (c.is_start ? "[CTRL] start" : "[CTRL] stop")
                  << ", sr=" << c.sample_rate
                  << ", ch=" << static_cast<int>(c.channels)
                  << ", br=" << c.bit_rate
                  << ", aot=" << static_cast<int>(c.aot)
                  << std::endl;
    });

    // 数据回调：直接把 ADTS（含头）写入文件
    std::atomic<uint64_t> totalBytes{0};
    std::atomic<uint64_t> totalFrames{0};
    streamer.onData([&](const AdtsStreamData& d){
        if (!d.payload.empty()) {
            ofs.write(reinterpret_cast<const char*>(d.payload.data()), static_cast<std::streamsize>(d.payload.size()));
            totalBytes += d.payload.size();
            totalFrames += d.frame_count;
        }
    });

    if (!streamer.start(cfg)) {
        std::cerr << "Failed to start MicrophoneAdtsStreamer (check tinyalsa device & fdk-aac)." << std::endl;
        return 3;
    }

    std::cout << "Recording to " << outPath << " for " << secs << "s... (Ctrl+C to stop)" << std::endl;

    const auto t0 = std::chrono::steady_clock::now();
    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - t0).count();
        if (secs > 0 && elapsed >= secs) break;
    }

    streamer.stop();
    ofs.flush();
    ofs.close();

    std::cout << "Done. frames=" << totalFrames.load()
              << ", bytes=" << totalBytes.load()
              << ", file=" << outPath << std::endl;

    return 0;
}
