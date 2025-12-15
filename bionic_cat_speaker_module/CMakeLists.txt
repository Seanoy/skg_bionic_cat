cmake_minimum_required(VERSION 3.10)
project(speaker_module LANGUAGES C CXX)

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


add_executable(speaker_module ${SERIAL_ADAPTER_SRC})
# 公开头文件
target_include_directories(speaker_module PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link third-party imported targets (these carry their own include dirs)
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    paho_mqtt_cpp::paho_mqtt_cpp
    paho_mqtt_c::paho_mqtt_c
    tinyalsa::tinyalsa
    ${CMAKE_SOURCE_DIR}/3rd/openssl/lib/libssl.so
    ${CMAKE_SOURCE_DIR}/3rd/openssl/lib/libcrypto.so
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

