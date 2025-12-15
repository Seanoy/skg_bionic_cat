# ================================================================
#  FindThirdPartyPro.cmake (v2)
#  自动检测 3rd 目录中的第三方库
#  - 兼容 include 子目录（如 include/mqtt/...）
#  - 自动导出 :: 命名空间 target
#  - 宽容模式：找不到库只打印警告，不中断
# ================================================================

function(find_third_party_pro)
    set(THIRD_PARTY_ROOT "${CMAKE_SOURCE_DIR}/3rd" CACHE PATH "3rd-party root directory")
    message(STATUS "🔍 Searching 3rd-party dependencies in: ${THIRD_PARTY_ROOT}")

    # 定义要检测的库（可自由增删）
    set(_libs
        zlib
        openssl
        glog
        mosquitto
        paho_mqtt_c
        paho_mqtt_cpp
        cmsis_dsp
        yaml_cpp
        tinyalsa
        fdk_aac
        cvi_mpi
        ini
        lvgl
        agora_sdk
        cJSON
        file_parser
    )

    foreach(_lib IN LISTS _libs)
        set(_lib_root "${THIRD_PARTY_ROOT}/${_lib}")
       message(STATUS "Searching ${_lib_root}>>>>")
        # === 路径检查 ===
        if(NOT EXISTS "${_lib_root}")
            message(WARNING "⚠ ${_lib_root} not found")
            continue()
        endif()

        # === include 目录自动检测 ===
        set(_include_dir "")
        if(EXISTS "${_lib_root}/include")
            # 直接存在 include 目录
            set(_include_dir "${_lib_root}/include")
        else()
            # 递归搜索第一个头文件，自动回溯到 include 根
            file(GLOB_RECURSE _headers "${_lib_root}/*.h" "${_lib_root}/*.hpp")
            if(_headers)
                list(GET _headers 0 _hdr)
                get_filename_component(_hdr_dir "${_hdr}" DIRECTORY)
                # 若路径中包含 “/include/”，回退到 include 根目录
                string(REGEX MATCH "(.*/include)" _match "${_hdr_dir}")
                if(_match)
                    set(_include_dir "${_match}")
                else()
                    # fallback：使用头文件所在目录
                    set(_include_dir "${_hdr_dir}")
                endif()
            endif()
        endif()

        if(NOT _include_dir)
            message(WARNING "⚠ No include directory found for ${_lib}")
        endif()

        # === lib 文件自动检测 ===
        file(GLOB_RECURSE _lib_files
            "${_lib_root}/lib/*.so"
            "${_lib_root}/lib/*.a"
            "${_lib_root}/lib64/*.so"
            "${_lib_root}/lib64/*.a"
        )

        if(NOT _lib_files)
            message(WARNING "⚠ No library files found for ${_lib}")
            continue()
        endif()

        list(GET _lib_files 0 _lib_file)

        # === 缓存导出变量 ===
        set(${_lib}_INCLUDE_DIRS "${_include_dir}" CACHE PATH "${_lib} include paths")
        set(${_lib}_LIBRARIES "${_lib_file}" CACHE FILEPATH "${_lib} library file")
        message(STATUS ${_lib}_INCLUDE_DIRS)
        message(STATUS ${_lib}_LIBRARIES)
        # === 创建标准 IMPORTED target ===
        if(NOT TARGET ${_lib}::${_lib})
            add_library(${_lib}::${_lib} UNKNOWN IMPORTED)
            set_target_properties(${_lib}::${_lib} PROPERTIES
                IMPORTED_LOCATION "${_lib_file}"
                INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}"
            )
        endif()

        if (NOT TARGET ${_lib})
            add_library(${_lib} UNKNOWN IMPORTED)
            set_target_properties(${_lib} PROPERTIES
                IMPORTED_LOCATION "${_lib_file}"
                INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}"
            )
        endif()

        # glog requires this definition so logging.h includes export macros correctly
        if(_lib STREQUAL "glog")
            set_property(TARGET ${_lib}::${_lib} APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS GLOG_USE_GLOG_EXPORT)
            set_property(TARGET ${_lib} APPEND PROPERTY
                INTERFACE_COMPILE_DEFINITIONS GLOG_USE_GLOG_EXPORT)
        endif()

        message(STATUS "✅ Found ${_lib}:")
        message(STATUS "    • Include: ${_include_dir}")
        message(STATUS "    • Library: ${_lib_file}")
    endforeach()

    message(STATUS "🎯 Third-party detection completed.")
endfunction()

find_third_party_pro()

