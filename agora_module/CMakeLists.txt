cmake_minimum_required(VERSION 3.10)
project(agora_module C)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rd/agora_sdk/include
    ${CMAKE_SOURCE_DIR}/3rd/cJSON/include
    ${CMAKE_SOURCE_DIR}/3rd/openssl/include
    ${CMAKE_SOURCE_DIR}/3rd/file_parser/include
    ${CMAKE_SOURCE_DIR}/3rd/fdk_aac/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utility
)

file(GLOB SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/utility/*.c
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    ${BIONIC_CAT_MQTT_MSGS_LIBRARIES}
    paho_mqtt_c::paho_mqtt_c
    tinyalsa::tinyalsa
    file_parser
    fdk_aac
    ${CMAKE_SOURCE_DIR}/3rd/agora_sdk/lib/libagora-rtc-sdk.so
    ${CMAKE_SOURCE_DIR}/3rd/cJSON/lib/libcjson.so
    ${CMAKE_SOURCE_DIR}/3rd/cJSON/lib/libcjson_utils.so
    ${CMAKE_SOURCE_DIR}/3rd/openssl/lib/libssl.so
    ${CMAKE_SOURCE_DIR}/3rd/openssl/lib/libcrypto.so
)
