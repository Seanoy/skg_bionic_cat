#!/bin/sh
# =====================================================
# 自动配置 3rd 库环境脚本（兼容 /bin/sh）
# 会从当前路径向上查找名为 3rd 的目录作为 THIRD_PARTY_ROOT
# 若已手动设置 THIRD_PARTY_ROOT，则优先使用该值
# =====================================================

# 0) 允许通过参数传入 3rd 或其父目录，并纠正传入为工程根目录的情况
if [ -n "$1" ]; then
    if [ -d "$1/3rd" ]; then
        THIRD_PARTY_ROOT="$1/3rd"
    elif [ -d "$1" ]; then
        THIRD_PARTY_ROOT="$1"
    fi
fi
# 若用户提前导出了工程根目录，自动纠正为其下的 3rd
if [ -n "$THIRD_PARTY_ROOT" ] && [ -d "$THIRD_PARTY_ROOT/3rd" ]; then
    THIRD_PARTY_ROOT="$THIRD_PARTY_ROOT/3rd"
fi

# 1) 确定 THIRD_PARTY_ROOT
if [ -n "$THIRD_PARTY_ROOT" ] && [ -d "$THIRD_PARTY_ROOT" ]; then
    : # 使用用户已设置的 THIRD_PARTY_ROOT
else
    # 在 bash 下可用 BASH_SOURCE 拿到脚本路径；在 sh 下回退到当前工作目录
    if [ -n "$BASH_SOURCE" ]; then
        BASE_DIR="$(cd "$(dirname "$BASH_SOURCE")" 2>/dev/null || pwd)"
    else
        BASE_DIR="$PWD"
    fi

    # 从 BASE_DIR 向上查找 3rd 目录
    SEARCH_DIR="$BASE_DIR"
    FOUND_3RD=""
    while [ -n "$SEARCH_DIR" ] && [ "$SEARCH_DIR" != "/" ]; do
        if [ -d "$SEARCH_DIR/3rd" ]; then
            FOUND_3RD="$SEARCH_DIR/3rd"
            break
        fi
        SEARCH_DIR="$(dirname "$SEARCH_DIR")"
    done

    if [ -n "$FOUND_3RD" ]; then
        THIRD_PARTY_ROOT="$FOUND_3RD"
    else
        # 兜底：若未找到 3rd，则用 BASE_DIR（可能不会匹配到任何库路径）
        THIRD_PARTY_ROOT="$BASE_DIR"
    fi
fi
export THIRD_PARTY_ROOT

# 2) 收集 include/lib/pkgconfig 路径
INCLUDE_PATHS=""
LIB_PATHS=""
PKG_PATHS=""

for dir in "$THIRD_PARTY_ROOT"/*; do
    [ -d "$dir" ] || continue
    if [ -d "$dir/include" ]; then
        INCLUDE_PATHS="$INCLUDE_PATHS:$dir/include"
    fi
    if [ -d "$dir/lib" ]; then
        LIB_PATHS="$LIB_PATHS:$dir/lib"
        if [ -d "$dir/lib/pkgconfig" ]; then
            PKG_PATHS="$PKG_PATHS:$dir/lib/pkgconfig"
        fi
    fi
done

# 去掉可能的前导冒号
INCLUDE_PATHS="${INCLUDE_PATHS#:}"
LIB_PATHS="${LIB_PATHS#:}"
PKG_PATHS="${PKG_PATHS#:}"

# 3) 安全地导出环境变量（避免出现多余的冒号）
if [ -n "$INCLUDE_PATHS" ]; then
    if [ -n "$CPLUS_INCLUDE_PATH" ]; then
        export CPLUS_INCLUDE_PATH="$INCLUDE_PATHS:$CPLUS_INCLUDE_PATH"
    else
        export CPLUS_INCLUDE_PATH="$INCLUDE_PATHS"
    fi
else
    : # 不修改 CPLUS_INCLUDE_PATH
fi
export C_INCLUDE_PATH="$CPLUS_INCLUDE_PATH"

if [ -n "$LIB_PATHS" ]; then
    if [ -n "$LIBRARY_PATH" ]; then
        export LIBRARY_PATH="$LIB_PATHS:$LIBRARY_PATH"
    else
        export LIBRARY_PATH="$LIB_PATHS"
    fi
    if [ -n "$LD_LIBRARY_PATH" ]; then
        export LD_LIBRARY_PATH="$LIB_PATHS:$LD_LIBRARY_PATH"
    else
        export LD_LIBRARY_PATH="$LIB_PATHS"
    fi
fi

if [ -n "$PKG_PATHS" ]; then
    if [ -n "$PKG_CONFIG_PATH" ]; then
        export PKG_CONFIG_PATH="$PKG_PATHS:$PKG_CONFIG_PATH"
    else
        export PKG_CONFIG_PATH="$PKG_PATHS"
    fi
fi

# 若未收集到任何 lib 路径，给出提示
if [ -z "$LIB_PATHS" ]; then
    echo "[提示] 未找到任何 3rd 库目录。若你看到 THIRD_PARTY_ROOT 不是以 /3rd 结尾，请执行：. ./envsetup.sh /mnt/data/nfs/sopgo_app/3rd" 1>&2
fi

# 4) 打印结果
echo "环境已配置完成！"
echo "THIRD_PARTY_ROOT=$THIRD_PARTY_ROOT"
echo "CPLUS_INCLUDE_PATH=${CPLUS_INCLUDE_PATH:-}"
echo "LIBRARY_PATH=${LIBRARY_PATH:-}"
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}"
echo "PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}"

