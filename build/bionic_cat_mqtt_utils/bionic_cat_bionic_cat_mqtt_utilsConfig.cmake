# bionic_cat_alarm_mqtt_utils_utilsConfig.cmake
# Config file for bionic_cat_alarm_mqtt_utils package



set(BIONIC_CAT_MQTT_UTILS_UTILS_VERSION 1.0.0)

# Include directories
set(BIONIC_CAT_MQTT_UTILS_INCLUDE_DIRS "/usr/local/include")

# Libraries
set(BIONIC_CAT_UTILS_LIBRARIES bionic_cat_alarm_mqtt_utils)

# Check if the library exists
include(CMakeFindDependencyMacro)
find_dependency(PahoMqttCpp REQUIRED)

# Import the targets
include("${CMAKE_CURRENT_LIST_DIR}/bionic_cat_mqtt_utilsTargets.cmake")

check_required_components(bionic_cat_mqtt_utils)
