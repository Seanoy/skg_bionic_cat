#!/bin/sh

echo "初始化SDK环境变量"
chmod +x .././scripts/envsetup.sh
source .././scripts/envsetup.sh CV184X DEPLOY
echo "初始化第三方库路径"
chmod +x ./scripts/envsetup.sh
source ./scripts/envsetup.sh
echo "启动毫米波雷达服务"
chmod +x ./scripts/open_radar.sh
./scripts/open_radar.sh
echo "启动应用程序"
chmod +x ./3rd/mosquitto/sbin/mosquitto
./3rd/mosquitto/sbin/mosquitto  &