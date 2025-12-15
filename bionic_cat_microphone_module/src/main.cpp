#include "microphone_node.hpp"
#include <csignal>
#include <iostream>
#include <memory>


const std::string SERVER_ADDRESS{"tcp://localhost:1883"};
const std::string CLIENT_ID{"microphone_node"};
const std::string PUBLISH_TOPIC{"bionic_cat/microphone_audio_frame"}; // Publish audio frames
const std::string PUBLISH_TOPIC_SOUND{"bionic_cat/sound_localization"}; // Publish sound localization results
const std::string SUBSCRIBE_TOPIC{"bionic_cat/microphone_control"}; // Subscribe to control commands
const int QOS = 1;


static std::unique_ptr<BionicCat::MicrophoneModule::MicrophoneNode> g_node;

static void on_signal(int) {
    if (g_node) g_node->stop();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    int CARD = 1;
    int DEVICE = 0;
    // 简单参数解析：支持 -d <card> / --card=<n> 和 -D <device> / --device=<n>
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            std::cout << "Usage: " << argv[0] << " [-d <card>] [-D <device>]\n"
                      << "  -d, --card   serial device (default:0)\n"
                      << "  -D, --device     baud rate (default: 0)\n";
            return 0;
        } else if (a == "-d" && i + 1 < argc) {
            CARD = std::stoi(argv[++i]);
        } else if (a.rfind("--card=", 0) == 0) {
            CARD = std::stoi(a.substr(std::string("--card=").size()));
        } else if (a == "-D" && i + 1 < argc) {
            DEVICE = std::stoi(argv[++i]);
        } else if (a.rfind("--device=", 0) == 0) {
            DEVICE = std::stoi(a.substr(std::string("--device=").size()));
        }
    }

    g_node = std::make_unique<BionicCat::MicrophoneModule::MicrophoneNode>(
        SERVER_ADDRESS, CLIENT_ID , PUBLISH_TOPIC, SUBSCRIBE_TOPIC, QOS, CARD, DEVICE, "A");

    if (!g_node->init()) {
        std::cerr << "[MicrophoneMain] init failed" << std::endl;
        return 1;
    }

    g_node->run();

    g_node->stop();
    g_node.reset();
    return 0;
}
