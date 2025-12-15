# skg_bionic_cat_cv1842h_a53_framework

## 编译前准备

### 修改 `cmake/CrossCompileConfig.cmake`
- 修改 `CROSS_COMPILE_BIN` 的地址为你的交叉编译工具链地址。

### 检查 `cmake/FindThirdPartyPro.cmake`
- 确认 `THIRD_PARTY_ROOT` 路径是否为正确的 3rd 目录。

## 编译指南

- 下载仓库。
- 放入 TDL_SDK 根目录中。
- 在 TDL_SDK 顶层 `CMakeLists.txt` 中添加：
  ```bash
  add_subdirectory(skg_bionic_cat_cv1842h_a53_framework)
  ```

## 添加第三方库

- 将交叉编译后的三方库放入 `3rd/` 目录中。
- 将库名添加到 `cmake/FindThirdPartyPro.cmake`：
  ```cmake
  # 定义要检测的库（可自由增删）
  set(_libs
      glog
      mosquitto
      paho_mqtt_c
      paho_mqtt_cpp
      cmsis_dsp
      yaml_cpp
  )
  ```

## 引用第三方库

- 在子模块 `CMakeLists.txt` 中添加：
  ```cmake
  target_link_libraries(${PROJECT_NAME} 
      INTERFACE 
      paho_mqtt_cpp::paho_mqtt_cpp  # 自己编译的库
      paho_mqtt_c::paho_mqtt_c      # 自己编译的库
      ${OPENSSL_LIBRARY}            # SDK库
  )
  ```

## 目录结构

```
skg_bionic_cat_cv1842h_a53_framework/
├── 3rd/                        # 第三方库
├── bionic_cat_action_controller_module/
├── bionic_cat_mqtt_msgs/
├── bionic_cat_mqtt_utils/
├── bionic_cat_radar_module/
├── bionic_cat_serial_adapter_module/
├── bionic_cat_vision_module/
├── cmake/
├── scripts/
├── CMakeLists.txt
└── README.md
```

## 主要模块说明

- **bionic_cat_mqtt_msgs**  
  定义与“bionic_cat”相关的 MQTT 消息结构体、枚举和常量，便于各模块间通过 MQTT 进行通信。  
  - 消息头文件位于 `include/` 目录下，主要定义了各类消息的数据结构和接口。
  - 支持常见的设备状态、控制指令、传感器数据等消息类型。
  - 统一消息头 `Header` 包含 `frame_id`、`device_id`、`timestamp`。
  - 枚举类型包括设备ID、电源请求、IMU状态、充电状态、温度异常状态等。
  - 典型消息结构体有 `HeartbeatMsg`、`MotorControlMsg`、`TempControlMsg`、`PowerControlMsg` 等。

- **bionic_cat_mqtt_utils**  
  提供基于 Paho MQTT C++ 库的 MQTT 发布/订阅工具类，支持 QoS、自动重连、多主题订阅等功能，便于各模块快速集成 MQTT 通信。

- **bionic_cat_serial_adapter_module / bionic_cat_action_controller_module / bionic_cat_radar_module / bionic_cat_vision_module**  
  各功能模块，分别实现串口适配、动作控制、雷达数据处理、视觉推理等功能，均可通过 MQTT 消息进行交互。


## 使用方法

1. 在你的模块 `CMakeLists.txt` 中添加依赖：
   ```cmake
   target_include_directories(<your_target> PRIVATE ${BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS})
   target_link_libraries(<your_target> PRIVATE ${BIONIC_CAT_MQTT_MSGS_LIBRARIES})
   ```
2. 在代码中包含头文件并使用相关消息结构体：
   ```cpp
   #include "bionic_cat_mqtt_msg.hpp"
   ```

## 适用场景
- skg_bionic_cat 项目

## 维护说明

