#ifndef MICROPHONE_NODE_HPP
#define MICROPHONE_NODE_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>

#include "mqtt_client.hpp"
#include "bionic_cat_mqtt_msg.hpp"
#include "serializer.hpp"
#include "capture_audio.hpp"

namespace BionicCat {
namespace MicrophoneModule {

class MicrophoneNode {
public:
    MicrophoneNode(const std::string& server_address,
                   const std::string& client_id,
                   const std::string& publish_topic_data,
                   const std::string& subscribe_topic_control,
                   int qos,
                   int card = 0,
                   int device = 0,
                   std::string device_id = "A");
    ~MicrophoneNode();

    bool init();
    void run();
    void stop();

    // 新增：发布声源定位结果（外部可调用）
    void publishSoundLocalization(const sound_localization_result& msg);
    // 新增：设置声源定位结果发布主题
    //void setSoundLocalizationTopic(const std::string& topic) { publish_topic_sound_ = topic; }

private:
    void handleControl(mqtt::const_message_ptr msg);
    void startStream(uint32_t sample_rate, uint8_t channels, uint32_t bitrate, uint8_t aot);
    void stopStream();

    struct ControlCmd { bool start; uint32_t sr; uint8_t ch; uint32_t br; uint8_t aot; };
    void controlLoop();

private:
    std::string server_address_;
    std::string client_id_;
    std::string publish_topic_data_;
    std::string subscribe_topic_control_;
    std::string publish_topic_sound_{"bionic_cat/sound_localization"}; // 新增：声源定位发布主题
    int qos_;
    int card_;
    int device_;
    std::string device_id_;

    std::atomic<bool> running_{false};

    std::unique_ptr<BionicCat::MqttClient::MQTTPublisher> publisher_;
    std::unique_ptr<BionicCat::MqttClient::MQTTSubscriber> subscriber_;
    std::unique_ptr<BionicCat::MqttClient::MQTTPublisher> sound_publisher_; // 新增：声源定位发布者

    std::mutex stream_mtx_;
    std::unique_ptr<MicrophoneAdtsStreamer> streamer_;

    std::thread control_thread_;
    std::mutex control_mtx_;
    std::condition_variable control_cv_;
    std::queue<ControlCmd> control_queue_;
};

} // namespace MicrophoneModule
} // namespace BionicCat

#endif // MICROPHONE_NODE_HPP
