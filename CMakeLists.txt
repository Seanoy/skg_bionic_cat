cmake_minimum_required(VERSION 3.16.3)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
project(bionic_cat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Ensure DEPS_ROOT points to the framework's 3rd-party folder so
# the FindCrossCompile* helpers can find cross-compiled deps by default.

#添加此工程需要的第三方库检测
# Option to control whether to use the cross-compiler setup and include
# the project-provided FindThirdPartyPro.cmake. Default ON for backward
# compatibility.
option(USE_CROSS_COMPLIER "Use cross compiler and enable FindThirdPartyPro" ON) #Used for building on x86_64
# set(PAHO_CPP_TARGET paho_mqtt_cpp::paho_mqtt_cpp)
# set(PAHO_C_TARGET paho_mqtt_c::paho_mqtt_c)

if(USE_CROSS_COMPLIER)
	message(STATUS "USE_CROSS_COMPLIER is ON")
	include(cmake/CrossCompileConfig.cmake)
	include(cmake/FindThirdPartyPro.cmake)
	
else()
	# message(STATUS "USE_CROSS_COMPLIER is OFF")
    # find_package(PahoMqttCpp REQUIRED)
	# find_package(Libwebsockets REQUIRED)
	# find_package(yaml-cpp REQUIRED)
	# find_package(glog REQUIRED)
endif()
	
# if(NOT TARGET ${PAHO_CPP_TARGET})
# 	message(WARNING "Target paho_mqtt_cpp::paho_mqtt_cpp NOT found, using PahoMqttCpp::paho-mqttpp3-shared instead")
# 	set(PAHO_CPP_TARGET PahoMqttCpp::paho-mqttpp3-shared)
# endif()
# if (NOT TARGET ${PAHO_C_TARGET})
# 	message(WARNING "Target paho_mqtt_c::paho_mqtt_c NOT found, using PahoMqttC::paho-mqtt3c-shared instead")
# 	set(PAHO_C_TARGET PahoMqttC::paho-mqtt3c-shared)
# endif()

# if(CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
#     set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install)
# endif()
# message(STATUS "INSTALL_PREFIX: " ${CMAKE_INSTALL_PREFIX})

# Build bionic_cat_mqtt_msgs first (header-only library)
add_subdirectory(bionic_cat_mqtt_msgs)

# Build bionic_cat_mqtt_utils (depends on bionic_cat_mqtt_msgs)
add_subdirectory(bionic_cat_mqtt_utils)

# Build the nodes
# add_subdirectory(bionic_cat_speaker_module)
# add_subdirectory(bionic_cat_microphone_module)
# add_subdirectory(bionic_cat_sound_source_localization_test)
add_subdirectory(agora_module)

# Install 3rd party libraries to installation target
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rd
    DESTINATION bionic_cat
    FILES_MATCHING PATTERN "*"
)

# 安装 scripts 目录到安装目标
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scripts
    DESTINATION bionic_cat
    FILES_MATCHING PATTERN "*"
)

# Print build summary
message(STATUS "===========================================")
message(STATUS "BIONIC CAT Framework Build Summary")
message(STATUS "===========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  - alarm_mqtt_msgs (header-only library)")
message(STATUS "  - alarm_mqtt_utils (library)")
message(STATUS "  - serial_adapter_module (node)")
message(STATUS "  - action_controller_module (node)")
message(STATUS "===========================================")
