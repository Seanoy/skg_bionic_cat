# CMSIS_DSP CMake configuration file



set(CMSIS_DSP_VERSION "1.16.2")

# Compute the installation prefix relative to this file
get_filename_component(CMSIS_DSP_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(CMSIS_DSP_PREFIX "${CMSIS_DSP_CMAKE_DIR}" PATH)
get_filename_component(CMSIS_DSP_PREFIX "${CMSIS_DSP_PREFIX}" PATH)

# Set include directories
set(CMSIS_DSP_INCLUDE_DIRS 
    "${CMSIS_DSP_PREFIX}/include/cmsis-dsp"
    "${CMSIS_DSP_PREFIX}/include/cmsis-core"
)

# Set library directories
set(CMSIS_DSP_LIBRARY_DIRS "${CMSIS_DSP_PREFIX}/lib")

# Find the library
find_library(CMSIS_DSP_LIBRARY
    NAMES CMSISDSP libCMSISDSP
    PATHS ${CMSIS_DSP_LIBRARY_DIRS}
    NO_DEFAULT_PATH
)

if(CMSIS_DSP_LIBRARY)
    set(CMSIS_DSP_LIBRARIES ${CMSIS_DSP_LIBRARY})
    set(CMSIS_DSP_FOUND TRUE)
else()
    set(CMSIS_DSP_FOUND FALSE)
endif()

# Create imported target if library is found - use lowercase target name
if(CMSIS_DSP_FOUND AND NOT TARGET CMSIS_DSP::cmsisdsp)
    add_library(CMSIS_DSP::cmsisdsp STATIC IMPORTED)
    set_target_properties(CMSIS_DSP::cmsisdsp PROPERTIES
        IMPORTED_LOCATION "${CMSIS_DSP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CMSIS_DSP_INCLUDE_DIRS}"
    )
endif()

# Check required components
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CMSIS_DSP
    REQUIRED_VARS CMSIS_DSP_LIBRARY CMSIS_DSP_INCLUDE_DIRS
    VERSION_VAR CMSIS_DSP_VERSION
)

# Mark variables as advanced
mark_as_advanced(CMSIS_DSP_LIBRARY CMSIS_DSP_INCLUDE_DIRS CMSIS_DSP_LIBRARIES)
