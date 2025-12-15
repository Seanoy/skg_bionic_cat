# waterworld_alarm_mqtt_utils

A C++ header-only library providing simple and easy-to-use MQTT publisher and subscriber classes built on top of the Paho MQTT C++ library.

## Features

- **Header-only library** - Easy integration into your projects
- **MQTTPublisher** - Simple interface for publishing messages to MQTT broker
- **MQTTSubscriber** - Simple interface for subscribing to topics and receiving messages
- **Flexible callbacks** - Customizable message and connection lost handlers
- **QoS support** - Support for Quality of Service levels 0, 1, and 2
- **Automatic reconnection** - Built-in reconnection handling
- **Multi-topic subscription** - Subscribe to multiple topics at once
- **Thread-safe** - Uses Paho's async client for thread safety

## Prerequisites

- C++11 or later
- CMake 3.10 or later
- [Paho MQTT C++ library](https://github.com/eclipse/paho.mqtt.cpp)

## Installation

### Install Paho MQTT C++ Library

First, install the Paho MQTT C and C++ libraries:

```bash
# Install Paho MQTT C library
git clone https://github.com/eclipse/paho.mqtt.c.git
cd paho.mqtt.c
mkdir build && cd build
cmake -DPAHO_WITH_SSL=ON -DPAHO_BUILD_STATIC=ON ..
sudo make install

# Install Paho MQTT C++ library
git clone https://github.com/eclipse/paho.mqtt.cpp.git
cd paho.mqtt.cpp
mkdir build && cd build
cmake -DPAHO_BUILD_STATIC=ON ..
sudo make install
```

### Build alarm_mqtt_utils

```bash
cd alarm_mqtt_utils
mkdir build && cd build
cmake ..
make
```

## Usage

### Basic Publisher Example

```cpp
#include "mqtt_client.hpp"

int main() {
    // Create publisher
    waterworld::alarm::MQTTPublisher publisher(
        "tcp://localhost:1883",  // Broker address
        "my_publisher_client",   // Client ID
        1                         // Default QoS
    );
    
    // Connect to broker
    if (!publisher.connect()) {
        return 1;
    }
    
    // Publish a message
    publisher.publish("test/topic", "Hello, MQTT!");
    
    // Disconnect
    publisher.disconnect();
    
    return 0;
}
```

### Basic Subscriber Example

```cpp
#include "mqtt_client.hpp"

int main() {
    // Create subscriber
    waterworld::alarm::MQTTSubscriber subscriber(
        "tcp://localhost:1883",  // Broker address
        "my_subscriber_client",  // Client ID
        1                         // Default QoS
    );
    
    // Optional: Set custom message handler
    subscriber.setMessageHandler([](mqtt::const_message_ptr msg) {
        std::cout << "Topic: " << msg->get_topic() 
                  << ", Message: " << msg->to_string() << std::endl;
    });
    
    // Connect to broker
    if (!subscriber.connect()) {
        return 1;
    }
    
    // Subscribe to topic
    if (!subscriber.subscribe("test/topic")) {
        subscriber.disconnect();
        return 1;
    }
    
    // Wait for messages...
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    // Cleanup
    subscriber.unsubscribe("test/topic");
    subscriber.disconnect();
    
    return 0;
}
```

## API Reference

### MQTTPublisher

#### Constructor
```cpp
MQTTPublisher(const std::string& serverAddress, 
              const std::string& clientId,
              int defaultQos = 1)
```

#### Methods
- `bool connect()` - Connect to MQTT broker
- `bool publish(const std::string& topic, const std::string& payload, int qos = -1, bool retained = false)` - Publish a message
- `mqtt::delivery_token_ptr publishAsync(...)` - Publish asynchronously
- `void disconnect()` - Disconnect from broker
- `bool isConnected() const` - Check connection status
- `void setAuth(const std::string& username, const std::string& password)` - Set authentication
- `void setSSL(const mqtt::ssl_options& sslOpts)` - Configure SSL/TLS

### MQTTSubscriber

#### Constructor
```cpp
MQTTSubscriber(const std::string& serverAddress,
               const std::string& clientId,
               int defaultQos = 1)
```

#### Methods
- `bool connect()` - Connect to MQTT broker
- `bool subscribe(const std::string& topic, int qos = -1)` - Subscribe to a topic
- `bool subscribe(const std::vector<std::string>& topics, int qos = -1)` - Subscribe to multiple topics
- `bool unsubscribe(const std::string& topic)` - Unsubscribe from a topic
- `void disconnect()` - Disconnect from broker
- `bool isConnected() const` - Check connection status
- `int getMessageCount() const` - Get number of messages received
- `void resetMessageCount()` - Reset message counter
- `void setMessageHandler(std::function<void(mqtt::const_message_ptr)> handler)` - Set custom message handler
- `void setConnectionLostHandler(std::function<void(const std::string&)> handler)` - Set connection lost handler
- `void setAuth(const std::string& username, const std::string& password)` - Set authentication
- `void setSSL(const mqtt::ssl_options& sslOpts)` - Configure SSL/TLS

## Building Examples

The library includes example programs demonstrating publisher and subscriber usage:

```bash
cd alarm_mqtt_utils/build
make
./bin/mqtt_publisher_example    # Run publisher example
./bin/mqtt_subscriber_example   # Run subscriber example
```

## Integration into Your Project

### Using CMake

Add this to your `CMakeLists.txt`:

```cmake
# Find the library
find_package(waterworld_alarm_mqtt_utils REQUIRED)

# Link against your target
target_link_libraries(your_target PRIVATE waterworld::waterworld_alarm_mqtt_utils)
```

### Direct Include

Simply include the header in your source files:

```cpp
#include "mqtt_client.hpp"
```

Make sure to link against the Paho MQTT C++ library (`-lpaho-mqttpp3 -lpaho-mqtt3as`).

## Project Structure

```
alarm_mqtt_utils/
├── CMakeLists.txt              # Main CMake configuration
├── README.md                   # This file
├── include/
│   └── mqtt_client.hpp         # Main header file with MQTTPublisher and MQTTSubscriber
├── examples/
│   ├── CMakeLists.txt          # Examples CMake configuration
│   ├── publisher_example.cpp   # Publisher example program
│   └── subscriber_example.cpp  # Subscriber example program
└── cmake/
    └── waterworld_alarm_mqtt_utilsConfig.cmake.in
```

## Testing with MQTT Broker

To test the examples, you need a running MQTT broker. You can use Mosquitto:

```bash
# Install Mosquitto
sudo apt-get install mosquitto mosquitto-clients

# Start broker
sudo systemctl start mosquitto

# Or run manually
mosquitto -v
```

## Advanced Features

### Multiple Topics Subscription

```cpp
std::vector<std::string> topics = {"topic1", "topic2", "topic3"};
subscriber.subscribe(topics, 1);
```

### Custom Message Handler

```cpp
subscriber.setMessageHandler([](mqtt::const_message_ptr msg) {
    // Custom processing
    std::string topic = msg->get_topic();
    std::string payload = msg->to_string();
    // ... your logic here
});
```

### Connection Lost Handler

```cpp
subscriber.setConnectionLostHandler([](const std::string& cause) {
    std::cerr << "Connection lost: " << cause << std::endl;
    // ... reconnection logic
});
```

### Retained Messages

```cpp
publisher.publish("status/online", "true", 1, true);  // Retained message
```

## License

This project is part of the Waterworld Alarm Framework.

## Contributing

Contributions are welcome! Please submit pull requests or open issues for bug reports and feature requests.