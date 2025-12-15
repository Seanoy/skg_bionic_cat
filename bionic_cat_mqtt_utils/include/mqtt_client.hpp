#ifndef MQTT_CLIENT_HPP
#define MQTT_CLIENT_HPP

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <mqtt/async_client.h>

namespace BionicCat {
namespace MqttClient {

/**
 * @brief Callback class for MQTT message handling
 * 
 * This class implements the mqtt::callback interface to handle
 * incoming messages, connection lost events, and message delivery.
 */
class MQTTCallback : public virtual mqtt::callback {
private:
    std::atomic<int> messageCount_;
    std::function<void(mqtt::const_message_ptr)> messageHandler_;
    std::function<void(const std::string&)> connectionLostHandler_;

public:
    MQTTCallback() : messageCount_(0) {}

    /**
     * @brief Set custom message handler
     * @param handler Function to be called when a message arrives
     */
    void setMessageHandler(std::function<void(mqtt::const_message_ptr)> handler) {
        messageHandler_ = handler;
    }

    /**
     * @brief Set custom connection lost handler
     * @param handler Function to be called when connection is lost
     */
    void setConnectionLostHandler(std::function<void(const std::string&)> handler) {
        connectionLostHandler_ = handler;
    }

    /**
     * @brief Called when connection to broker is lost
     */
    void connection_lost(const std::string& cause) override {
        std::cout << "\n*** Connection lost ***" << std::endl;
        if (!cause.empty()) {
            std::cout << "Cause: " << cause << std::endl;
        }
        
        if (connectionLostHandler_) {
            connectionLostHandler_(cause);
        }
    }

    /**
     * @brief Called when a message arrives
     */
    void message_arrived(mqtt::const_message_ptr msg) override {
        messageCount_++;
        
        if (messageHandler_) {
            messageHandler_(msg);
        } else {
            // Default message handling
            std::cout << "\n=== Message Arrived ===" << std::endl;
            std::cout << "Topic: " << msg->get_topic() << std::endl;
            std::cout << "Payload: " << msg->to_string() << std::endl;
            std::cout << "QoS: " << msg->get_qos() << std::endl;
            std::cout << "Retained: " << (msg->is_retained() ? "true" : "false") << std::endl;
            std::cout << "Message count: " << messageCount_ << std::endl;
            std::cout << "=======================" << std::endl;
        }
    }

    /**
     * @brief Called when message delivery is complete
     */
    void delivery_complete(mqtt::delivery_token_ptr token) override {
        // Can be extended if needed
    }

    int getMessageCount() const {
        return messageCount_;
    }

    void resetMessageCount() {
        messageCount_ = 0;
    }
};

/**
 * @brief MQTT Publisher Class
 * 
 * This class provides a simple interface to publish messages to an MQTT broker.
 * It handles connection management and message publishing with QoS support.
 */
class MQTTPublisher {
private:
    mqtt::async_client client_;
    mqtt::connect_options connOpts_;
    std::string serverAddress_;
    int defaultQos_;

public:
    /**
     * @brief Construct a new MQTTPublisher
     * @param serverAddress MQTT broker address (e.g., "tcp://localhost:1883")
     * @param clientId Unique client identifier
     * @param defaultQos Default Quality of Service level (0, 1, or 2)
     */
    MQTTPublisher(const std::string& serverAddress, 
                  const std::string& clientId,
                  int defaultQos = 1)
        : client_(serverAddress, clientId)
        , serverAddress_(serverAddress)
        , defaultQos_(defaultQos) {
        
        // Configure connection options
        connOpts_.set_keep_alive_interval(20);
        connOpts_.set_clean_session(true);
        connOpts_.set_automatic_reconnect(true);
    }

    /**
     * @brief Set username and password for authentication
     */
    void setAuth(const std::string& username, const std::string& password) {
        connOpts_.set_user_name(username);
        connOpts_.set_password(password);
    }

    /**
     * @brief Configure SSL/TLS options
     */
    void setSSL(const mqtt::ssl_options& sslOpts) {
        connOpts_.set_ssl(sslOpts);
    }

    /**
     * @brief Connect to the MQTT broker
     * @return true if connection successful, false otherwise
     */
    bool connect() {
        try {
            std::cout << "Connecting to MQTT broker at " << serverAddress_ << "..." << std::endl;
            
            mqtt::token_ptr conntok = client_.connect(connOpts_);
            conntok->wait();
            
            std::cout << "Connected successfully!" << std::endl;
            return true;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error connecting: " << exc.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Publish a message to the specified topic
     * @param topic Topic to publish to
     * @param payload Message payload
     * @param qos Quality of Service level (optional, uses default if not specified)
     * @param retained Whether the message should be retained by the broker
     * @return true if publish successful, false otherwise
     */
    bool publish(const std::string& topic, 
                 const std::string& payload,
                 int qos = -1,
                 bool retained = false) {
        try {
            int actualQos = (qos < 0) ? defaultQos_ : qos;
            
            auto msg = mqtt::make_message(topic, payload);
            msg->set_qos(actualQos);
            msg->set_retained(retained);
            
            mqtt::token_ptr pubtok = client_.publish(msg);
            pubtok->wait();
            
            return true;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error publishing: " << exc.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Publish a message asynchronously
     * @param topic Topic to publish to
     * @param payload Message payload
     * @param qos Quality of Service level (optional, uses default if not specified)
     * @param retained Whether the message should be retained by the broker
     * @return Delivery token for tracking the publish operation
     */
    mqtt::delivery_token_ptr publishAsync(const std::string& topic,
                                          const std::string& payload,
                                          int qos = -1,
                                          bool retained = false) {
        int actualQos = (qos < 0) ? defaultQos_ : qos;
        
        auto msg = mqtt::make_message(topic, payload);
        msg->set_qos(actualQos);
        msg->set_retained(retained);
        
        return client_.publish(msg);
    }

    /**
     * @brief Disconnect from the MQTT broker
     */
    void disconnect() {
        try {
            std::cout << "Disconnecting..." << std::endl;
            mqtt::token_ptr disctok = client_.disconnect();
            disctok->wait();
            std::cout << "Disconnected successfully!" << std::endl;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error disconnecting: " << exc.what() << std::endl;
        }
    }

    /**
     * @brief Check if connected to broker
     * @return true if connected, false otherwise
     */
    bool isConnected() const {
        return client_.is_connected();
    }

    /**
     * @brief Get the underlying MQTT client
     * @return Reference to the async_client
     */
    mqtt::async_client& getClient() {
        return client_;
    }
};

/**
 * @brief MQTT Subscriber Class
 * 
 * This class provides a simple interface to subscribe to topics and receive
 * messages from an MQTT broker. It handles connection management, topic
 * subscription, and message callbacks.
 */
class MQTTSubscriber {
private:
    mqtt::async_client client_;
    mqtt::connect_options connOpts_;
    MQTTCallback callback_;
    std::string serverAddress_;
    int defaultQos_;

public:
    /**
     * @brief Construct a new MQTTSubscriber
     * @param serverAddress MQTT broker address (e.g., "tcp://localhost:1883")
     * @param clientId Unique client identifier
     * @param defaultQos Default Quality of Service level (0, 1, or 2)
     */
    MQTTSubscriber(const std::string& serverAddress,
                   const std::string& clientId,
                   int defaultQos = 1)
        : client_(serverAddress, clientId)
        , serverAddress_(serverAddress)
        , defaultQos_(defaultQos) {
        
        // Configure connection options
        connOpts_.set_keep_alive_interval(20);
        connOpts_.set_clean_session(true);
        connOpts_.set_automatic_reconnect(true);
        
        // Set callback for message handling
        client_.set_callback(callback_);
    }

    /**
     * @brief Set username and password for authentication
     */
    void setAuth(const std::string& username, const std::string& password) {
        connOpts_.set_user_name(username);
        connOpts_.set_password(password);
    }

    /**
     * @brief Configure SSL/TLS options
     */
    void setSSL(const mqtt::ssl_options& sslOpts) {
        connOpts_.set_ssl(sslOpts);
    }

    /**
     * @brief Set custom message handler
     * @param handler Function to be called when a message arrives
     */
    void setMessageHandler(std::function<void(mqtt::const_message_ptr)> handler) {
        callback_.setMessageHandler(handler);
    }

    /**
     * @brief Set custom connection lost handler
     * @param handler Function to be called when connection is lost
     */
    void setConnectionLostHandler(std::function<void(const std::string&)> handler) {
        callback_.setConnectionLostHandler(handler);
    }

    /**
     * @brief Connect to the MQTT broker
     * @return true if connection successful, false otherwise
     */
    bool connect() {
        try {
            std::cout << "Connecting to MQTT broker at " << serverAddress_ << "..." << std::endl;
            
            mqtt::token_ptr conntok = client_.connect(connOpts_);
            conntok->wait();
            
            std::cout << "Connected successfully!" << std::endl;
            return true;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error connecting: " << exc.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Subscribe to a topic
     * @param topic Topic to subscribe to
     * @param qos Quality of Service level (optional, uses default if not specified)
     * @return true if subscription successful, false otherwise
     */
    bool subscribe(const std::string& topic, int qos = -1) {
        try {
            int actualQos = (qos < 0) ? defaultQos_ : qos;
            std::cout << "Subscribing to topic: " << topic << " (QoS " << actualQos << ")" << std::endl;
            
            mqtt::token_ptr subtok = client_.subscribe(topic, actualQos);
            subtok->wait();
            
            std::cout << "Subscribed successfully!" << std::endl;
            return true;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error subscribing: " << exc.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Subscribe to multiple topics
     * @param topics Vector of topics to subscribe to
     * @param qos Quality of Service level for all topics
     * @return true if subscription successful, false otherwise
     */
    bool subscribe(const std::vector<std::string>& topics, int qos = -1) {
        try {
            int actualQos = (qos < 0) ? defaultQos_ : qos;
            std::vector<int> qosLevels(topics.size(), actualQos);
            
            std::cout << "Subscribing to " << topics.size() << " topics (QoS " << actualQos << ")..." << std::endl;
            
            // Convert std::vector<std::string> to mqtt::const_string_collection_ptr
            auto topicCollection = mqtt::string_collection::create(topics);
            mqtt::token_ptr subtok = client_.subscribe(topicCollection, qosLevels);
            subtok->wait();
            
            std::cout << "Subscribed successfully!" << std::endl;
            return true;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error subscribing: " << exc.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Unsubscribe from a topic
     * @param topic Topic to unsubscribe from
     * @return true if unsubscription successful, false otherwise
     */
    bool unsubscribe(const std::string& topic) {
        try {
            std::cout << "Unsubscribing from topic: " << topic << std::endl;
            
            mqtt::token_ptr unsubtok = client_.unsubscribe(topic);
            unsubtok->wait();
            
            std::cout << "Unsubscribed successfully!" << std::endl;
            return true;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error unsubscribing: " << exc.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Disconnect from the MQTT broker
     */
    void disconnect() {
        try {
            std::cout << "\nDisconnecting..." << std::endl;
            mqtt::token_ptr disctok = client_.disconnect();
            disctok->wait();
            std::cout << "Disconnected successfully!" << std::endl;
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error disconnecting: " << exc.what() << std::endl;
        }
    }

    /**
     * @brief Check if connected to broker
     * @return true if connected, false otherwise
     */
    bool isConnected() const {
        return client_.is_connected();
    }

    /**
     * @brief Get the number of messages received
     * @return Message count
     */
    int getMessageCount() const {
        return callback_.getMessageCount();
    }

    /**
     * @brief Reset the message counter
     */
    void resetMessageCount() {
        callback_.resetMessageCount();
    }

    /**
     * @brief Get the underlying MQTT client
     * @return Reference to the async_client
     */
    mqtt::async_client& getClient() {
        return client_;
    }
};

} // namespace alarm
} // namespace waterworld

#endif // MQTT_CLIENT_HPP
