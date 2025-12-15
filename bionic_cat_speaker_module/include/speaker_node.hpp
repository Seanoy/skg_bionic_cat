#ifndef SPEAKER_NODE_HPP
#define SPEAKER_NODE_HPP

#include <string>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <iostream>
#include "mqtt_client.hpp"
#include "bionic_cat_mqtt_msg.hpp"
#include "play_wav_tinyalsa.hpp"

namespace BionicCat {
namespace SpeakerModule {

class SpeakerNode {
public:
    SpeakerNode(const std::string& server_address,
                const std::string& client_id,
                const std::string& subscribe_topic,
                int qos,
                int card = 0,
                int device = 0);

    ~SpeakerNode();

    bool init();
    void run();
    void stop();

private:
    void handleAudioPlayCommand(mqtt::const_message_ptr msg);
    void playCommand(const BionicCat::MqttMsgs::AudioPlayCommand& cmd);

    std::string server_address_;
    std::string client_id_;
    std::string subscribe_topic_;
    int qos_;
    std::atomic<bool> running_;

    std::unique_ptr<BionicCat::MqttClient::MQTTSubscriber> subscriber_;
    std::unique_ptr<WavPlayer> player_;

    int current_card_ = 0;
    int current_device_ = 0;

};

} // namespace SpeakerModule
} // namespace BionicCat

#endif // SPEAKER_NODE_HPP
