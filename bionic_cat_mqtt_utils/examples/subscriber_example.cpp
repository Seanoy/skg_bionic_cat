#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <chrono>
#include "../include/mqtt_client.hpp"

// MQTT broker address
const std::string SERVER_ADDRESS{"tcp://localhost:1883"};
const std::string CLIENT_ID{"paho_cpp_subscriber"};
const std::string TOPIC{"skg/test"};

// QoS level
const int QOS = 1;

/**
 * @brief Simple MQTT Subscriber Example
 * 
 * This example demonstrates how to:
 * - Connect to an MQTT broker
 * - Subscribe to a topic
 * - Handle incoming messages via callbacks
 * - Gracefully disconnect
 */
int main(int argc, char* argv[]) {
    std::cout << "=== Paho MQTT C++ Subscriber Example ===" << std::endl;
    
    // Create subscriber instance using the library class
    BionicCat::MqttClient::MQTTSubscriber subscriber(SERVER_ADDRESS, CLIENT_ID, QOS);
    
    // Optional: Set custom message handler
    // If not set, the default handler will print message details
    /*
    subscriber.setMessageHandler([](mqtt::const_message_ptr msg) {
        std::cout << "Custom handler - Topic: " << msg->get_topic() 
                  << ", Payload: " << msg->to_string() << std::endl;
    });
    */
    
    // Connect to broker
    if (!subscriber.connect()) {
        return 1;
    }

    // Subscribe to topic
    if (!subscriber.subscribe(TOPIC)) {
        subscriber.disconnect();
        return 1;
    }

    std::cout << "\nWaiting for messages..." << std::endl;
    std::cout << "Press Ctrl+C to stop\n" << std::endl;

    // Keep the subscriber running for a period of time
    // In a real application, this would run until interrupted
    const int RUN_DURATION_SECONDS = 60;
    
    for (int i = 0; i < RUN_DURATION_SECONDS && subscriber.isConnected(); ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\nReceived " << subscriber.getMessageCount() << " messages total." << std::endl;

    // Unsubscribe and disconnect
    subscriber.unsubscribe(TOPIC);
    subscriber.disconnect();

    std::cout << "Subscriber finished." << std::endl;
    return 0;
}
