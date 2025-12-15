cmake_minimum_required(VERSION 3.10)
project(bionic_cat_mqtt_msgs)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Option to build tests
option(BUILD_TESTS "Build test programs" OFF)

# Collect all header files
file(GLOB GENERATED_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

# Include the generated files in the include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add header-only library
add_library(${PROJECT_NAME} INTERFACE)

# Target include directories
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Export these variables for use by other CMakeLists.txt
set(BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE INTERNAL "Include directories for bionic_cat_mqtt_msgs")
set(BIONIC_CAT_MQTT_MSGS_LIBRARIES ${PROJECT_NAME} CACHE INTERNAL "Libraries for bionic_cat_mqtt_msgs")

# Build tests if enabled
if(BUILD_TESTS)
    message(STATUS "Building bionic_cat_mqtt_msgs tests...")
    
    # Add test executable
    add_executable(test_serializer test/test_serializer.cpp)
    
    # Link against the header-only library
    target_include_directories(test_serializer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    
    # Set output directory
    set_target_properties(test_serializer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # Install test executable
    install(TARGETS test_serializer
        RUNTIME DESTINATION bin
    )
    
    message(STATUS "  Test executable: test_serializer")
endif()

message(STATUS "bionic_cat_mqtt_msgs configured:")
message(STATUS "  Include dirs: ${BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
