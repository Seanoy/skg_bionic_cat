#include "microphone_node.hpp"

#include <chrono>
#include <iostream>

#include "mqtt_utils.hpp"
namespace BionicCat {
namespace MicrophoneModule {

using BionicCat::MqttMsgs::AdtsStreamControlMsg;
using BionicCat::MqttMsgs::AdtsStreamDataMsg;
using BionicCat::MqttMsgs::Header;
using BionicCat::MqttMsgs::SoundLocalizationMsg;

MicrophoneNode::MicrophoneNode(const std::string& server_address,
                               const std::string& client_id,
                               const std::string& publish_topic_data,
                               const std::string& subscribe_topic_control,
                               int qos,
                               int card,
                               int device,
                               std::string device_id)
    : server_address_(server_address)
    , client_id_(client_id)
    , publish_topic_data_(publish_topic_data)
    , subscribe_topic_control_(subscribe_topic_control)
    , qos_(qos)
    , card_(card)
    , device_(device)
    , device_id_(std::move(device_id)) {}

MicrophoneNode::~MicrophoneNode() {
    stop();
}

bool MicrophoneNode::init() {
    publisher_ = std::make_unique<BionicCat::MqttClient::MQTTPublisher>(server_address_, client_id_ + "_pub", qos_);
    subscriber_ = std::make_unique<BionicCat::MqttClient::MQTTSubscriber>(server_address_, client_id_ + "_sub", qos_);
    // 新增：定位发布者
    sound_publisher_ = std::make_unique<BionicCat::MqttClient::MQTTPublisher>(server_address_, client_id_ + "_sound_pub", qos_);

    if (!publisher_->connect()) {
        std::cerr << "[MicrophoneNode] Failed to connect publisher" << std::endl;
        return false;
    }
    if (!sound_publisher_->connect()) {
        std::cerr << "[MicrophoneNode] Failed to connect sound publisher" << std::endl;
        return false;
    }

    subscriber_->setMessageHandler(std::bind(&MicrophoneNode::handleControl, this, std::placeholders::_1));
    if (!subscriber_->connect()) {
        std::cerr << "[MicrophoneNode] Failed to connect subscriber" << std::endl;
        return false;
    }
    if (!subscriber_->subscribe(subscribe_topic_control_)) {
        std::cerr << "[MicrophoneNode] Failed to subscribe control topic: " << subscribe_topic_control_ << std::endl;
        return false;
    }

    running_.store(true);

    // start control worker
    control_thread_ = std::thread(&MicrophoneNode::controlLoop, this);

    return true;
}

void MicrophoneNode::run() {
    std::cout << "[MicrophoneNode] Running" << std::endl;
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

void MicrophoneNode::stop() {
    running_.store(false);

    // wake and join control thread
    {
        std::lock_guard<std::mutex> lk(control_mtx_);
        // push a sentinel stop command (optional)
        control_cv_.notify_all();
    }
    if (control_thread_.joinable()) control_thread_.join();

    stopStream();
    if (subscriber_) subscriber_->disconnect();
    if (publisher_) publisher_->disconnect();
    if (sound_publisher_) sound_publisher_->disconnect();
}

void MicrophoneNode::handleControl(mqtt::const_message_ptr msg) {
    try {
        const auto& payload = msg->get_payload();
        auto ctrl = BionicCat::MsgsSerializer::Serializer::deserializeAdtsStreamControl(
            reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
        // std::cout << "[MicrophoneNode] Received control command: "
        //           << (ctrl.is_start ? "START" : "STOP")
        //           << ", sr=" << ctrl.sample_rate
        //           << ", ch=" << int(ctrl.channels)
        //           << ", br=" << ctrl.bit_rate
        //           << ", aot=" << int(ctrl.aot) << std::endl;
        // enqueue control command
        ControlCmd cmd{ctrl.is_start, ctrl.sample_rate, ctrl.channels, ctrl.bit_rate, ctrl.aot};
        {
            std::lock_guard<std::mutex> lk(control_mtx_);
            control_queue_.push(cmd);
        }
        control_cv_.notify_one();
    } catch (const std::exception& e) {
        std::cerr << "[MicrophoneNode] control parse error: " << e.what() << std::endl;
    }
}

void MicrophoneNode::controlLoop() {
    while (running_.load()) {
        ControlCmd cmd;
        {
            std::unique_lock<std::mutex> lk(control_mtx_);
            control_cv_.wait(lk, [this]{ return !running_.load() || !control_queue_.empty(); });
            if (!running_.load()) break;
            cmd = control_queue_.front();
            control_queue_.pop();
        }

        if (cmd.start) {
            std::cout << "[MicrophoneNode] Received START command" << std::endl;
            startStream(cmd.sr, cmd.ch, cmd.br, cmd.aot);
        } else {
            std::cout << "[MicrophoneNode] Received STOP command" << std::endl;
            stopStream();
        }

        
    }
}

void MicrophoneNode::startStream(uint32_t sample_rate, uint8_t channels, uint32_t bitrate, uint8_t aot) {
    std::lock_guard<std::mutex> lk(stream_mtx_);
    if (streamer_) {
        // already running, ignore duplicate start
        std::cout << "[MicrophoneNode] Stream already running, ignoring start" << std::endl;
        return;
    }

    streamer_ = std::make_unique<MicrophoneAdtsStreamer>();

    // 数据回调：发布到 publish_topic_data_
    streamer_->onData([this](const AdtsStreamData& d){
        AdtsStreamDataMsg m{};
        m.header.frame_id = "microphone";
        m.header.device_id = device_id_;
        m.header.timestamp = getStamp();
        m.seq = d.seq;
        m.pts_ms = d.pts_ms;
        m.frame_count = d.frame_count;
        m.payload = d.payload;
        auto bin = BionicCat::MsgsSerializer::Serializer::serializeAdtsStreamData(m);
        publisher_->publish(publish_topic_data_, std::string(reinterpret_cast<const char*>(bin.data()), bin.size()), qos_, false);
    });

    // 新增：声源定位回调，直接发布
    streamer_->onLocalization([this](const sound_localization_result& msg){
        publishSoundLocalization(msg);
    });

    // 控制回调（仅打印）
    streamer_->onControl([this](const AdtsStreamControl& c){
        std::cout << "[MicrophoneNode] stream " << (c.is_start?"start":"stop")
                  << ", sr=" << c.sample_rate
                  << ", ch=" << int(c.channels)
                  << ", br=" << c.bit_rate
                  << ", aot=" << int(c.aot) << std::endl;
    });

    MicrophoneAdtsStreamer::Config cfg;
    cfg.card = card_;
    cfg.device = device_;
    cfg.sample_rate = sample_rate;
    cfg.channels = channels;
    cfg.bit_rate = bitrate;
    cfg.aot = aot;
    // 开启定位由外部设置 publish_topic_sound_ 与 cfg.enable_localization 等，这里保留默认关闭

    if (!streamer_->start(cfg)) {
        std::cerr << "[MicrophoneNode] start stream failed" << std::endl;
        streamer_.reset();
        return;
    }
}

void MicrophoneNode::stopStream() {
    std::lock_guard<std::mutex> lk(stream_mtx_);
    if (!streamer_) return;
    streamer_->stop();
    streamer_.reset();
}

void MicrophoneNode::publishSoundLocalization(const sound_localization_result& msg) {
    if (!sound_publisher_) return;
    if (publish_topic_sound_.empty()) return;
    BionicCat::MqttMsgs::SoundLocalizationMsg m;
    m.header.frame_id = "microphone";
    m.header.device_id = device_id_;
    m.header.timestamp = getStamp();
    m.azimuth_deg = msg.azimuth;
    m.elevation_deg = msg.elevation;
    m.confidence = msg.confidence;
    m.loudness[0] = msg.loudness[0];
    m.loudness[1] = msg.loudness[1];
    m.loudness[2] = msg.loudness[2];
    m.loudness[3] = msg.loudness[3];
    // std::cout << "[MicrophoneNode] Publishing sound localization: azimuth=" << m.azimuth_deg
    //           << ", elevation=" << m.elevation_deg
    //           << ", confidence=" << m.confidence
    //           << ", loudness[0]=" << m.loudness[0]
    //           << ", loudness[1]=" << m.loudness[1]
    //           << ", loudness[2]=" << m.loudness[2]
    //           << ", loudness[3]=" << m.loudness[3] << std::endl;
    auto bin = BionicCat::MsgsSerializer::Serializer::serializeSoundLocalization(m);
    sound_publisher_->publish(publish_topic_sound_, std::string(reinterpret_cast<const char*>(bin.data()), bin.size()), qos_, false);
}

} // namespace MicrophoneModule
} // namespace BionicCat
