#include "speaker_node.hpp"
#include <csignal>
#include <memory>
#include <iostream>

const std::string SERVER_ADDRESS{"tcp://localhost:1883"};
const std::string CLIENT_ID{"speaker_node"};
const std::string SUBSCRIBE_TOPIC{"bionic_cat/audio_play_command"};
const int QOS = 1;

std::unique_ptr<BionicCat::SpeakerModule::SpeakerNode> g_node;

void signalHandler(int signum) {
    std::cout << "\n[SpeakerNode] Signal " << signum << " received, shutting down..." << std::endl;
    if (g_node) g_node->stop();
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    g_node = std::make_unique<BionicCat::SpeakerModule::SpeakerNode>(
        SERVER_ADDRESS, CLIENT_ID, SUBSCRIBE_TOPIC, QOS, 0, 0);

    if (!g_node->init()) {
        std::cerr << "[SpeakerNode] Initialization failed" << std::endl;
        return 1;
    }

    g_node->run();
    return 0;
}
