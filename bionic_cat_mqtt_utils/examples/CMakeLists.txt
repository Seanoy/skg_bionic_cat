cmake_minimum_required(VERSION 3.10)

# Note: PahoMqttCpp is found in root CMakeLists.txt

# Publisher example
add_executable(mqtt_publisher_example publisher_example.cpp)
target_include_directories(mqtt_publisher_example PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(mqtt_publisher_example PRIVATE 
    PahoMqttCpp::paho-mqttpp3-shared
)

# Subscriber example
add_executable(mqtt_subscriber_example subscriber_example.cpp)
target_include_directories(mqtt_subscriber_example PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(mqtt_subscriber_example PRIVATE 
    PahoMqttCpp::paho-mqttpp3-shared
)

# Set output directory for examples
set_target_properties(mqtt_publisher_example mqtt_subscriber_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install examples (optional)
install(TARGETS mqtt_publisher_example mqtt_subscriber_example
    RUNTIME DESTINATION bin
)
