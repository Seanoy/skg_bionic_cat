cmake_minimum_required(VERSION 3.10)
project(bionic_cat_bionic_cat_mqtt_utils VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all header files
file(GLOB GENERATED_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

# Header-only library interface
add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:include>
)

# Link third-party imported targets (these carry their own include dirs)
if (USE_CROSS_COMPLIER)
    target_link_libraries(${PROJECT_NAME} 
        INTERFACE 
        ${PAHO_C_TARGET}
        ${PAHO_CPP_TARGET}
        ${OPENSSL_LIBRARY}
    )
else()
    target_link_libraries(${PROJECT_NAME} 
        INTERFACE 
        # ${PAHO_C_TARGET}
        ${PAHO_CPP_TARGET}
        ${OPENSSL_LIBRARY}
    )
endif()

# Option to build examples
option(BUILD_EXAMPLES "Build example programs" OFF)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Export targets
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE bionic_cat::
    DESTINATION lib/cmake/${PROJECT_NAME}
)

# Create and install package config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_file(cmake/${PROJECT_NAME}Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    @ONLY
)

# Install the config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION lib/cmake/${PROJECT_NAME}
)

# Export these variables for use by other CMakeLists.txt
set(BIONIC_CAT_MQTT_UTILS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE INTERNAL "Include directories for bionic_cat_mqtt_utils")
set(BIONIC_CAT_UTILS_LIBRARIES ${PROJECT_NAME} CACHE INTERNAL "Libraries for bionic_cat_mqtt_utils")

message(STATUS "bionic_cat_mqtt_utils configured:")
message(STATUS "  Include dirs: ${BIONIC_CAT_MQTT_UTILS_INCLUDE_DIRS}")
message(STATUS "  Libraries: ${BIONIC_CAT_UTILS_LIBRARIES}")
