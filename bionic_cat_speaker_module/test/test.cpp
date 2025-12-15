#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include "mqtt_client.hpp"
#include "serializer.hpp"
#include "bionic_cat_mqtt_msg.hpp"

#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <csignal>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <functional>

using namespace BionicCat;

static const std::string SERVER_ADDRESS{"tcp://localhost:1883"};
static const std::string CLIENT_ID{"speaker_test_publisher"};
static const std::string TOPIC{"bionic_cat/audio_play_command"};
static const int QOS = 1;

static const int GPIO_PLAY = 353; // toggle play/pause
static const int GPIO_CTRL = 360; // short: next, long: previous
static const std::string AUDIO_DIR = "/mnt/data/audio/";

std::atomic<bool> g_stop{false};

void printUsage(const char* prog){
    std::cout << "Usage: " << prog << " <file_path> [--speed S] [--volume V] [--loop]" << std::endl;
    std::cout << "Example: " << prog << " /root/test.wav --speed 1.2 --volume 0.8 --loop" << std::endl;
}

static bool writeToFile(const std::string &path, const std::string &content) {
    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << content;
    return true;
}

static bool fileExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static bool ensureExportedGpio(int gpio) {
    std::string gpioPath = "/sys/class/gpio/gpio" + std::to_string(gpio);
    if (!fileExists(gpioPath)) {
        if (!writeToFile("/sys/class/gpio/export", std::to_string(gpio))) {
            // May already be exported or permission denied
            std::cerr << "Warning: cannot export gpio " << gpio << std::endl;
        }
        // wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // set direction to in
    std::string dirPath = gpioPath + "/direction";
    if (fileExists(dirPath)) {
        writeToFile(dirPath, "in");
    }
    return fileExists(gpioPath + "/value");
}

static int readGpioValue(int gpio) {
    std::string valPath = "/sys/class/gpio/gpio" + std::to_string(gpio) + "/value";
    std::ifstream ifs(valPath);
    if (!ifs) return -1;
    char c = '1';
    ifs >> c;
    return (c == '0') ? 0 : 1; // assume '0' is pressed for many boards
}

// Monitor button for single vs double short clicks (no long press).
// onSingle: called for single short click; onDouble: called when two short clicks detected within timeout.
static void monitorButtonClicks(int gpio, std::function<void()> onSingle, std::function<void()> onDouble) {
    if (!ensureExportedGpio(gpio)) {
        std::cerr << "Failed to prepare gpio " << gpio << std::endl;
        return;
    }
    int lastVal = readGpioValue(gpio);
    const int debounceMs = 30;
    const int doubleClickMs = 400; // max interval between clicks for double-click
    while (!g_stop.load()) {
        int v = readGpioValue(gpio);
        if (v < 0) { std::this_thread::sleep_for(std::chrono::milliseconds(200)); continue; }
        // detect edge: high->low (assuming active low button)
        if (v == 0 && lastVal != 0) {
            // debounce
            std::this_thread::sleep_for(std::chrono::milliseconds(debounceMs));
            if (readGpioValue(gpio) != 0) { lastVal = readGpioValue(gpio); continue; }
            // wait for release
            while (!g_stop.load() && readGpioValue(gpio) == 0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            // first click detected
            auto clickTime = std::chrono::steady_clock::now();
            bool isDouble = false;
            // wait for potential second click within timeout
            auto deadline = clickTime + std::chrono::milliseconds(doubleClickMs);
            while (std::chrono::steady_clock::now() < deadline && !g_stop.load()) {
                int vv = readGpioValue(gpio);
                if (vv == 0) {
                    // debounce second press
                    std::this_thread::sleep_for(std::chrono::milliseconds(debounceMs));
                    if (readGpioValue(gpio) == 0) {
                        // wait release of second
                        while (!g_stop.load() && readGpioValue(gpio) == 0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        isDouble = true;
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (isDouble) {
                if (onDouble) onDouble();
            } else {
                if (onSingle) onSingle();
            }
            lastVal = readGpioValue(gpio);
        } else {
            lastVal = v;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) { printUsage(argv[0]); return 1; }

    std::string file_path = argv[1];
    float speed = 1.0f;
    float volume = 1.0f;
    bool loop = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--speed" && i + 1 < argc) {
            speed = std::strtof(argv[++i], nullptr);
            if (speed <= 0.f) speed = 1.f;
        } else if (arg == "--volume" && i + 1 < argc) {
            volume = std::strtof(argv[++i], nullptr);
            if (volume < 0.f) volume = 0.f;
        } else if (arg == "--loop") {
            loop = true;
        } else {
            std::cerr << "Unknown arg: " << arg << std::endl;
        }
    }

    // Load playlist from AUDIO_DIR (.wav files)
    std::vector<std::string> playlist;
    DIR *dir = opendir(AUDIO_DIR.c_str());
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (ent->d_type != DT_REG && ent->d_type != DT_UNKNOWN) continue;
            std::string name = ent->d_name;
            std::string full = AUDIO_DIR + name;
            // if d_type is unknown, fall back to stat
            if (ent->d_type == DT_UNKNOWN) {
                struct stat st;
                if (stat(full.c_str(), &st) != 0) continue;
                if (!S_ISREG(st.st_mode)) continue;
            }
            auto pos = name.rfind('.');
            if (pos == std::string::npos) continue;
            std::string ext = name.substr(pos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".wav") playlist.push_back(full);
        }
        closedir(dir);
    } else {
        std::cerr << "Failed to open audio directory " << AUDIO_DIR << ": " << strerror(errno) << std::endl;
    }

    // 排序目录中的文件
    std::sort(playlist.begin(), playlist.end());

    // 将用户传入文件优先放入播放列表，并设置当前索引
    int preferredIndex = -1;
    if (fileExists(file_path)) {
        // 若不在列表中，则插入到列表头部；若在列表中，记录其位置
        auto it = std::find(playlist.begin(), playlist.end(), file_path);
        if (it == playlist.end()) {
            playlist.insert(playlist.begin(), file_path);
            preferredIndex = 0;
        } else {
            preferredIndex = static_cast<int>(std::distance(playlist.begin(), it));
        }
    }

    // 如果目录和传入文件都不可用
    if (playlist.empty()) {
        std::cerr << "No wav files found in " << AUDIO_DIR << " and provided file not found." << std::endl;
        return 1;
    }

    // MQTT publisher
    BionicCat::MqttClient::MQTTPublisher pub(SERVER_ADDRESS, CLIENT_ID, QOS);
    if (!pub.connect()) {
        std::cerr << "Failed to connect to MQTT broker" << std::endl;
        return 2;
    }

    std::atomic<int> current(0);
    if (preferredIndex >= 0) current.store(preferredIndex);
    std::atomic<bool> playing{true};

    auto publishTrack = [&](int idx, float playSpeed){
        if (idx < 0) idx = 0;
        if (idx >= (int)playlist.size()) idx = 0;
        MqttMsgs::AudioPlayCommand cmd;
        cmd.header.frame_id = "speaker_test";
        cmd.header.device_id = "A";
        cmd.header.timestamp = static_cast<int64_t>(time(nullptr)) * 1000;
        cmd.file_path = playlist[idx];
        cmd.speed = playSpeed;
        cmd.volume = volume;
        cmd.loop = loop;
        std::vector<uint8_t> payload = BionicCat::MsgsSerializer::Serializer::serializeAudioPlayCommand(cmd);
        std::string payload_str(payload.begin(), payload.end());
        bool ok = pub.publish(TOPIC, payload_str);
        std::cout << (ok ? "Published OK" : "Publish failed") << " -> " << cmd.file_path << " (speed=" << playSpeed << ")" << std::endl;
        return ok;
    };

    // publish first track (优先用户选择的文件)
    publishTrack(current.load(), speed);

    // button callbacks
    auto onPlayToggle = [&](){
        if (playing.load()) {
            // pause: publish with speed = 0
            publishTrack(current.load(), 0.0f);
            playing.store(false);
            std::cout << "Paused" << std::endl;
        } else {
            // resume
            publishTrack(current.load(), speed);
            playing.store(true);
            std::cout << "Resumed" << std::endl;
        }
    };
    auto onNext = [&](){
        int idx = current.fetch_add(1) + 1; // fetch_add returns previous
        if (idx >= (int)playlist.size()) idx = 0;
        current.store(idx);
        publishTrack(idx, speed);
    };
    auto onPrev = [&](){
        int idx = current.load() - 1;
        if (idx < 0) idx = (int)playlist.size() - 1;
        current.store(idx);
        publishTrack(idx, speed);
    };

    // start monitor threads (single vs double click behavior)
    std::thread t1([&](){ monitorButtonClicks(GPIO_PLAY, onPlayToggle, nullptr); });
    std::thread t2([&](){ monitorButtonClicks(GPIO_CTRL, onNext, onPrev); });

    // handle SIGINT to exit
    std::signal(SIGINT, [](int){ g_stop.store(true); });
    std::signal(SIGTERM, [](int){ g_stop.store(true); });

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (t1.joinable()) t1.join();
    if (t2.joinable()) t2.join();

    pub.disconnect();
    return 0;
}
