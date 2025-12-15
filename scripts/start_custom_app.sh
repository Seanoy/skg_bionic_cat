#!/bin/sh

# 1. 配置环境变量 (保持不变)
export LD_LIBRARY_PATH="/mnt/data/CV184X/bionic_cat/3rd/lvgl/lib:/mnt/data/CV184X/bionic_cat/3rd/cmsis_dsp/lib:/mnt/data/CV184X/bionic_cat/3rd/cvi_mpi/lib:/mnt/data/CV184X/bionic_cat/3rd/fdk_aac/lib:/mnt/data/CV184X/bionic_cat/3rd/glog/lib:/mnt/data/CV184X/bionic_cat/3rd/ini/lib:/mnt/data/CV184X/bionic_cat/3rd/mosquitto/lib:/mnt/data/CV184X/bionic_cat/3rd/paho_mqtt_c/lib:/mnt/data/CV184X/bionic_cat/3rd/paho_mqtt_cpp/lib:/mnt/data/CV184X/bionic_cat/3rd/tinyalsa/lib:/mnt/data/CV184X/bionic_cat/3rd/yaml_cpp/lib:/mnt/data/CV184X/lib:/mnt/data/CV184X/sample/3rd/opencv/lib:/mnt/data/CV184X/sample/utils/lib:/lib:/lib/3rd:/lib/arm-linux-gnueabihf:/usr/lib:/usr/local/lib:/system/lib:/system/usr/lib:/system/usr/lib/3rd:/mnt/data/lib"
export LIBRARY_PATH="/mnt/data/CV184X/bionic_cat/3rd/lvgl/lib:/mnt/data/CV184X/bionic_cat/3rd/cmsis_dsp/lib:/mnt/data/CV184X/bionic_cat/3rd/cvi_mpi/lib:/mnt/data/CV184X/bionic_cat/3rd/fdk_aac/lib:/mnt/data/CV184X/bionic_cat/3rd/glog/lib:/mnt/data/CV184X/bionic_cat/3rd/ini/lib:/mnt/data/CV184X/bionic_cat/3rd/mosquitto/lib:/mnt/data/CV184X/bionic_cat/3rd/paho_mqtt_c/lib:/mnt/data/CV184X/bionic_cat/3rd/paho_mqtt_cpp/lib:/mnt/data/CV184X/bionic_cat/3rd/tinyalsa/lib:/mnt/data/CV184X/bionic_cat/3rd/yaml_cpp/lib"
export SSL_CERT_FILE="/mnt/data/ssl/cacert.pem"

# 2. 设置清理陷阱 (核心优化)
# 当脚本退出(EXIT)时，自动 kill 所有当前 Shell 的后台作业(jobs -p)
# 2>/dev/null 用于防止没有后台进程时 kill 报错
trap 'echo "\n[INFO] Stopping all modules..."; kill $(jobs -p) 2>/dev/null; echo "[INFO] Done."' EXIT

# 3. 进入目录
cd /mnt/data/CV184X/bionic_cat/ || { echo "Directory not found"; exit 1; }

# 4. 获取程序列表
APPS="${@:-eyeball_module}"

echo "Preparing to start: $APPS"

# 5. 循环启动程序
for APP in $APPS; do
    # 处理路径
    if echo "$APP" | grep -q "^/"; then TARGET="$APP";
    elif echo "$APP" | grep -q "^\./"; then TARGET="$APP";
    else TARGET="./$APP"; fi

    if [ -f "$TARGET" ]; then
        echo "[INFO] Starting $TARGET ..."
        chmod +x "$TARGET"
        
        # 必须后台运行 (&)，否则 jobs -p 捕获不到，且脚本会卡住
        "$TARGET" & 
        
        echo "[INFO] Started (PID: $!)"
    else
        echo "[ERROR] Program '$TARGET' not found! Skipping."
    fi
done

echo "--------------------------------------"
echo "[INFO] Running. Press Ctrl+C to exit and stop all modules."

# 6. 挂起等待
# 这一步至关重要。如果没有 wait，脚本执行完循环就会直接退出(EXIT)，
# 进而触发 trap 立即把刚启动的程序全杀掉。
wait
