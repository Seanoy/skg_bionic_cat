# ================================================================
# FindCrossCompileLibWebsockets.cmake
# 自动检测交叉编译的 libwebsockets，位置期望为：
#   ${_deps}/libwebsockets-src
# 目录下包含 bin/ include/ lib/ share/ 等子目录
# 会导出以下变量与目标（宽容模式：找不到则警告但不报错）：
#   libwebsockets_INCLUDE_DIRS (CACHE PATH)
#   libwebsockets_LIBRARIES    (CACHE FILEPATH)
#   libwebsockets_BIN_DIR     (CACHE PATH, optional)
# 并创建 IMPORTED target: websockets
# ================================================================

function(find_cross_compile_libwebsockets)
    # 首选使用外部传入的变量 _deps（如果项目已有），否则尝试使用 DEPS_ROOT 或默认 3rd 目录
    if(DEFINED _deps AND _deps)
        set(_deps_root "${_deps}")
    elseif(DEFINED DEPS_ROOT AND DEPS_ROOT)
        set(_deps_root "${DEPS_ROOT}")
    else()
        set(_deps_root "${CMAKE_SOURCE_DIR}/skg_bionic_cat_cv1842h_a53_framework/3rd")
    endif()

    set(_lws_root "${_deps_root}/libwebsockets-src")
    message(STATUS "🔍 Searching cross-compiled libwebsockets in: ${_lws_root}")

    if(NOT EXISTS "${_lws_root}")
        message(WARNING "⚠ ${_lws_root} not found")
        return()
    endif()

    # 1) include 目录检测
    set(_include_dir "")
    if(EXISTS "${_lws_root}/include")
        set(_include_dir "${_lws_root}/include")
    else()
        file(GLOB_RECURSE _hdrs "${_lws_root}/*.h" "${_lws_root}/*.hpp")
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
        message(WARNING "⚠ No include directory found for libwebsockets under ${_lws_root}")
    endif()

    # 2) lib 文件检测（支持 lib/ lib64/ 深度子目录）
    file(GLOB_RECURSE _lib_files
        "${_lws_root}/lib/*.a" "${_lws_root}/lib/*.so" "${_lws_root}/lib/*.so.*"
        "${_lws_root}/lib64/*.a" "${_lws_root}/lib64/*.so" "${_lws_root}/lib64/*.so.*"
        "${_lws_root}/lib/**/*.a" "${_lws_root}/lib/**/*.so" "${_lws_root}/lib/**/*.so.*"
    )

    if(NOT _lib_files)
        message(WARNING "⚠ No library files found for libwebsockets under ${_lws_root}")
        return()
    endif()

    # 优先选择命名包含 libwebsockets 的库文件，其次取第一个匹配项
    set(_selected_lib "")
    foreach(_f IN LISTS _lib_files)
        get_filename_component(_bn "${_f}" NAME)
        if(_bn MATCHES "libwebsockets")
            set(_selected_lib "${_f}")
            break()
        endif()
    endforeach()
    if(NOT _selected_lib)
        list(GET _lib_files 0 _selected_lib)
    endif()

    # 3) 可选 bin 目录
    if(EXISTS "${_lws_root}/bin")
        set(_bin_dir "${_lws_root}/bin")
    endif()

    # 4) 导出到 cache 和 创建 IMPORTED target
    set(libwebsockets_INCLUDE_DIRS "${_include_dir}" CACHE PATH "libwebsockets include dir")
    set(libwebsockets_LIBRARIES "${_selected_lib}" CACHE FILEPATH "libwebsockets library file")
    if(_bin_dir)
        set(libwebsockets_BIN_DIR "${_bin_dir}" CACHE PATH "libwebsockets bin dir")
    endif()

    if(NOT TARGET websockets)
        add_library(websockets UNKNOWN IMPORTED)
        set_target_properties(websockets PROPERTIES
            IMPORTED_LOCATION "${_selected_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${_include_dir}"
        )
    endif()

    message(STATUS "✅ Found cross-compiled libwebsockets:")
    message(STATUS "    • Include: ${_include_dir}")
    message(STATUS "    • Library: ${_selected_lib}")
    if(_bin_dir)
        message(STATUS "    • Bin: ${_bin_dir}")
    endif()
endfunction()

# 自动运行，方便直接 include() 使用
find_cross_compile_libwebsockets()
