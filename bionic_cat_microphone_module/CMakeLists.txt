cmake_minimum_required(VERSION 3.10)
project(microphone_module LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 源文件
file(GLOB SERIAL_ADAPTER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)


add_executable(microphone_module ${SERIAL_ADAPTER_SRC})
# 公开头文件
target_include_directories(microphone_module PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link third-party imported targets (these carry their own include dirs)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    paho_mqtt_cpp::paho_mqtt_cpp
    paho_mqtt_c::paho_mqtt_c
    tinyalsa::tinyalsa
    fdk_aac::fdk_aac
    yaml_cpp::yaml_cpp
    ${CMAKE_SOURCE_DIR}/3rd/openssl/lib/libssl.so
    ${CMAKE_SOURCE_DIR}/3rd/openssl/lib/libcrypto.so
    zlib::zlib
)

# Link bionic_cat_mqtt_utils
if(BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS)
    target_include_directories(${PROJECT_NAME} PRIVATE ${BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${BIONIC_CAT_MQTT_MSGS_LIBRARIES})
endif() 

# Link bionic_cat_mqtt_utils
if(BIONIC_CAT_MQTT_UTILS_INCLUDE_DIRS)
    target_include_directories(${PROJECT_NAME} PRIVATE ${BIONIC_CAT_MQTT_UTILS_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${BIONIC_CAT_UTILS_LIBRARIES})
endif()


# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bionic_cat
)

option(BUILD_MICROPHONE_TESTS "Build minimal test executable for this module" OFF)
if(BUILD_MICROPHONE_TESTS)
    message(STATUS "Adding test target: microphone_streamer_test")
    add_executable(microphone_streamer_test
        ${CMAKE_CURRENT_SOURCE_DIR}/src/capture_audio.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/sound_localization.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/test_streamer.cpp
    )

    target_include_directories(microphone_streamer_test PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(microphone_streamer_test 
        PRIVATE 
        tinyalsa::tinyalsa
        fdk_aac::fdk_aac
        yaml_cpp::yaml_cpp
    )

    install(TARGETS microphone_streamer_test
        RUNTIME DESTINATION bionic_cat/test
    )

    add_executable(microphone_mqtt_test
        ${CMAKE_CURRENT_SOURCE_DIR}/test/test_microphone_mqtt.cpp
    )

    target_include_directories(microphone_mqtt_test PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(microphone_mqtt_test
        PRIVATE
        paho_mqtt_cpp::paho_mqtt_cpp
        paho_mqtt_c::paho_mqtt_c
    )

    if(BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS)
        target_include_directories(microphone_mqtt_test PRIVATE ${BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS})
        target_link_libraries(microphone_mqtt_test PRIVATE ${BIONIC_CAT_MQTT_MSGS_LIBRARIES})
    endif()
    if(BIONIC_CAT_MQTT_UTILS_INCLUDE_DIRS)
        target_include_directories(microphone_mqtt_test PRIVATE ${BIONIC_CAT_MQTT_UTILS_INCLUDE_DIRS})
        target_link_libraries(microphone_mqtt_test PRIVATE ${BIONIC_CAT_UTILS_LIBRARIES})
    endif()

    install(TARGETS microphone_mqtt_test
        RUNTIME DESTINATION bionic_cat/test
    )
endif()