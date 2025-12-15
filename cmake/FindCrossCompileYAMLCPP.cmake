# ================================================================
# FindCrossCompileYAMLCPP.cmake
# 自动检测交叉编译的 yaml-cpp，位置期望为：
#   ${_deps}/yaml-cpp-src
# 目录下包含 bin/ include/ lib/ share/ 等子目录
# 会导出以下变量与目标（宽容模式：找不到则警告但不报错）：
#   yaml_cpp_INCLUDE_DIRS (CACHE PATH)
#   yaml_cpp_LIBRARIES    (CACHE FILEPATH)
#   yaml_cpp_BIN_DIR     (CACHE PATH, optional)
# 并创建 IMPORTED target: yaml_cpp::yaml_cpp
# ================================================================

function(find_cross_compile_yaml_cpp)
    # 直接使用 3rd 目录下的 yaml_cpp
    set(_y_root "${CMAKE_CURRENT_SOURCE_DIR}/3rd/yaml_cpp")
    message(STATUS "🔍 Searching cross-compiled yaml-cpp in: ${_y_root}")

    if(NOT EXISTS "${_y_root}")
        message(WARNING "⚠ ${_y_root} not found")
        return()
    endif()

    # include 检测
    set(_include_dir "")
    if(EXISTS "${_y_root}/include")
        set(_include_dir "${_y_root}/include")
    else()
        file(GLOB_RECURSE _hdrs "${_y_root}/*.h" "${_y_root}/*.hpp")
        if(_hdrs)
            list(GET _hdrs 0 _first_hdr)
            get_filename_component(_hdr_dir "${_first_hdr}" DIRECTORY)
            string(REGEX MATCH "(.*/include)" _match "${_hdr_dir}")
            if(_match)
                set(_include_dir "${_match}")
            else()
                set(_include_dir "${_hdr_dir}")
            endif()
        endif()
    endif()

    if(NOT _include_dir)
        message(WARNING "⚠ No include directory found for yaml-cpp under ${_y_root}")
    endif()

    # lib 文件检测（支持 lib/ lib64/ 深度搜索）
    file(GLOB_RECURSE _lib_files
        "${_y_root}/lib/*.a" "${_y_root}/lib/*.so" "${_y_root}/lib/*.so.*"
        "${_y_root}/lib64/*.a" "${_y_root}/lib64/*.so" "${_y_root}/lib64/*.so.*"
        "${_y_root}/lib/**/*.a" "${_y_root}/lib/**/*.so" "${_y_root}/lib/**/*.so.*"
    )

    if(NOT _lib_files)
        message(WARNING "⚠ No library files found for yaml-cpp under ${_y_root}")
        return()
    endif()

    # 优先选择文件名包含 yaml 和 cpp 的库（例如 libyaml-cpp.a），否则使用第一个匹配
    set(_selected_lib "")
    foreach(_f IN LISTS _lib_files)
        get_filename_component(_bn "${_f}" NAME)
        string(TOLOWER _bn_lc "${_bn}")
        if(_bn_lc MATCHES "yaml" AND _bn_lc MATCHES "cpp")
            set(_selected_lib "${_f}")
            break()
        endif()
    endforeach()
    if(NOT _selected_lib)
        list(GET _lib_files 0 _selected_lib)
    endif()

    # 可选 bin
    if(EXISTS "${_y_root}/bin")
        set(_bin_dir "${_y_root}/bin")
    endif()

    # 导出 cache 变量与 IMPORTED target
    set(yaml_cpp_INCLUDE_DIRS "${_include_dir}" CACHE PATH "yaml-cpp include dir")
    set(yaml_cpp_LIBRARIES "${_selected_lib}" CACHE FILEPATH "yaml-cpp library file")
    if(_bin_dir)
        set(yaml_cpp_BIN_DIR "${_bin_dir}" CACHE PATH "yaml-cpp bin dir")
    endif()

    if(NOT TARGET yaml-cpp)
        add_library(yaml-cpp UNKNOWN IMPORTED)
        set_target_properties(yaml-cpp PROPERTIES
            IMPORTED_LOCATION "${_selected_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}"
        )
    endif()

    message(STATUS "✅ Found cross-compiled yaml-cpp:")
    message(STATUS "    • Include: ${_include_dir}")
    message(STATUS "    • Library: ${_selected_lib}")
    if(_bin_dir)
        message(STATUS "    • Bin: ${_bin_dir}")
    endif()
endfunction()

# 自动运行，方便直接 include() 使用
find_cross_compile_yaml_cpp()
