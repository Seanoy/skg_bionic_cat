#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "../include/mqtt_client.hpp"

// MQTT broker address
const std::string SERVER_ADDRESS{"tcp://localhost:1883"};
const std::string CLIENT_ID{"paho_cpp_publisher"};
const std::string TOPIC{"skg/test"};

// QoS level
const int QOS = 1;

/**
 * @brief Simple MQTT Publisher Example
 * 
 * This example demonstrates how to:
 * - Connect to an MQTT broker
 * - Publish messages to a topic
 * - Handle connection callbacks
 * - Gracefully disconnect
 */
int main(int argc, char* argv[]) {
    std::cout << "=== Paho MQTT C++ Publisher Example ===" << std::endl;
    
    // Create publisher instance using the library class
    BionicCat::MqttClient::MQTTPublisher publisher(SERVER_ADDRESS, CLIENT_ID, QOS);
    
    // Connect to broker
    if (!publisher.connect()) {
        return 1;
    }

    // Publish messages every 2 seconds
    int messageCount = 0;
    const int MAX_MESSAGES = 10;

    std::cout << "\nStarting to publish messages..." << std::endl;
    std::cout << "Press Ctrl+C to stop\n" << std::endl;

    while (publisher.isConnected() && messageCount < MAX_MESSAGES) {
        // Create message payload
        std::string payload = "Message #" + std::to_string(messageCount) + 
                              " from publisher at " + 
                              std::to_string(std::time(nullptr));
        
        // Publish message
        publisher.publish(TOPIC, payload);
        
        messageCount++;
        
        // Wait before publishing next message
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "\nPublished " << messageCount << " messages total." << std::endl;

    // Disconnect from broker
    publisher.disconnect();

    std::cout << "Publisher finished." << std::endl;
    return 0;
}
