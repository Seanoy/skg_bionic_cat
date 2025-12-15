#include "speaker_node.hpp"
#include "serializer.hpp"
#include <chrono>

namespace BionicCat {
namespace SpeakerModule {

SpeakerNode::SpeakerNode(const std::string& server_address,
                         const std::string& client_id,
                         const std::string& subscribe_topic,
                         int qos,
                         int card ,
                         int device)
    : server_address_(server_address)
    , client_id_(client_id)
    , subscribe_topic_(subscribe_topic)
    , qos_(qos)
    , running_(false) 
    , current_card_(card)
    , current_device_(device) {}

SpeakerNode::~SpeakerNode() {
    stop();
}

bool SpeakerNode::init() {
    subscriber_ = std::make_unique<BionicCat::MqttClient::MQTTSubscriber>(server_address_, client_id_ + "_audio_sub", qos_);
    subscriber_->setMessageHandler(std::bind(&SpeakerNode::handleAudioPlayCommand, this, std::placeholders::_1));
    std::cout << "[SpeakerNode] Connecting subscriber..." << std::endl;
    if (!subscriber_->connect()) {
        std::cerr << "[SpeakerNode] Failed to connect MQTT subscriber" << std::endl;
        return false;
    }
    if (!subscriber_->subscribe(subscribe_topic_)) {
        std::cerr << "[SpeakerNode] Failed to subscribe topic: " << subscribe_topic_ << std::endl;
        return false;
    }
    std::cout << "[SpeakerNode] Init OK. Subscribed to " << subscribe_topic_ << std::endl;
    running_ = true;
    return true;
}

void SpeakerNode::run() {
    std::cout << "[SpeakerNode] Running loop." << std::endl;
    while (running_ && subscriber_ && subscriber_->isConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void SpeakerNode::stop() {
    if (!running_) return;
    running_ = false;
    if (player_) {
        player_->stop();
        player_.reset();
    }
    if (subscriber_) subscriber_->disconnect();
}

void SpeakerNode::handleAudioPlayCommand(mqtt::const_message_ptr msg) {
    try {
        auto cmd = BionicCat::MsgsSerializer::Serializer::deserializeAudioPlayCommand(
            reinterpret_cast<const uint8_t*>(msg->get_payload().data()),
            msg->get_payload().size());
        std::cout << "[SpeakerNode] AudioPlayCommand received: file=" << cmd.file_path
                  << " speed=" << cmd.speed
                  << " volume=" << cmd.volume
                  << " loop=" << cmd.loop << std::endl;
        playCommand(cmd);
    } catch (const std::exception& e) {
        std::cerr << "[SpeakerNode] Failed to deserialize AudioPlayCommand: " << e.what() << std::endl;
    }
}

void SpeakerNode::playCommand(const BionicCat::MqttMsgs::AudioPlayCommand& cmd) {
    if (!player_) player_ = std::make_unique<WavPlayer>();
    player_->stop();
    player_->file_path = cmd.file_path;
    player_->speed = cmd.speed <= 0.f ? 1.f : cmd.speed;
    player_->volume = cmd.volume < 0.f ? 0.f : cmd.volume;
    player_->loop = cmd.loop;
    if (!player_->load(current_card_, current_device_)) { // default card/device; could be extended via header.device_id
        std::cerr << "[SpeakerNode] Failed to load wav: " << cmd.file_path << std::endl;
        return;
    }
    if (!player_->play()) {
        std::cerr << "[SpeakerNode] Failed to start playback" << std::endl;
    }
}

} // namespace SpeakerModule
} // namespace BionicCat


