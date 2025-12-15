# =====================================================
# Auto-generated ARM cross-compile CMake config
# =====================================================
include(CMakeFindDependencyMacro)

add_library(eclipse-paho-mqtt-c::paho-mqtt3as STATIC IMPORTED GLOBAL)
set_target_properties(eclipse-paho-mqtt-c::paho-mqtt3as PROPERTIES
    IMPORTED_LOCATION "/home/finnox/sophgo/tools/skg_dependencies/dependencies/paho.mqtt.c/build_arm/install/lib/libpaho-mqtt3as.a"
    INTERFACE_INCLUDE_DIRECTORIES "/home/finnox/sophgo/tools/skg_dependencies/dependencies/paho.mqtt.c/build_arm/install/include"
)
