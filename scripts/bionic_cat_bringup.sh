#!/bin/bash

PROGRAM_PATH=./install/bin
PROGRAMS_LIST=("mosquitto" "action_controller_module" "serial_adapter_module" )
TTY_DEVICE="/dev/ttyS1"
# 捕获终止信号（SIGINT和SIGTERM）

cd "$PROGRAM_PATH" || exit 1  # 确保进入目录成功

monitor_mosquitto() {
    local program=$1
    while true; do
        # 启动程序并等待退出
        mosquitto
        echo "$program has stopped. Restarting..."
        sleep 1  # 延时1秒再重启
    done
}

monitor_serial_adapter() {
    local program=$1
    while true; do
        # 启动程序并等待退出
        ./serial_adapter_module -d $TTY_DEVICE
        echo "$program has stopped. Restarting..."
        sleep 1  # 延时1秒再重启
    done
}

# 启动并监控每个程序
monitor_program() {
    local program=$1
    while true; do
        # 启动程序并等待退出
        cd "$PROGRAM_PATH"
            ./$program
        echo "$program has stopped. Restarting..."
        sleep 1  # 延时1秒再重启
    done
}

while true; do
    # 启动并监控程序列表中的每个程序
    for program in "${PROGRAMS_LIST[@]}"; do
        echo "启动程序: $program"
        if [ "$program" == "mosquitto" ]; then
            monitor_mosquitto &  # 使用后台任务监控程序
        elif [ "$program" == "alarm_mmw_drv" ]; then
            monitor_serial_adapter &  # 使用后台任务监控程序
        else
            monitor_program "$program" &  # 使用后台任务监控程序
        fi
    done
    wait  # 等待所有后台程序完成
    sleep 5
done

wait  # 等待所有后台程序完成
# =====================================================================================
