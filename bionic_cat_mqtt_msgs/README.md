# bionic_cat_mqtt_msgs

## 消息定义

本模块用于定义与“bionic_cat”相关的 MQTT 消息结构体、枚举和常量，便于各模块间通过 MQTT 进行通信。

### 主要内容

- 消息头文件位于 `include/` 目录下，主要定义了各类消息的数据结构和接口。
- 支持常见的设备状态、控制指令、传感器数据等消息类型。
- 通过统一的消息格式，提升了系统的可扩展性和模块间解耦。

### 消息结构说明

核心消息定义在 `include/bionic_cat_mqtt_msg.hpp`，主要内容如下：

- **统一消息头 Header**  
  包含 `frame_id`、`device_id`、`timestamp`，用于标识消息来源和时间。

- **枚举类型**  
  包括设备ID（`RobotId`）、电源请求（`APwrRequest`）、IMU状态（`BImuState`）、充电状态（`BChargeState`）、温度异常状态（`BTempErrorState`）等，便于设备间状态同步和控制。

- **A板发送消息（A → B）**  
  - `HeartbeatMsg`：心跳包，包含硬件/软件版本和时间戳  
  - `MotorControlMsg`：电机控制，包含目标位置和使能掩码  
  - `TempControlMsg`：温度控制  
  - `PowerControlMsg`：电源控制及重启参数

- **B板发送消息（B → A）**  
  - `TouchStatusMsg`：触摸状态及异常  
  - `MotorStatusMsg`：电机实际状态及异常  
  - `TempStatusMsg`：温度及异常  
  - `ImuStatusMsg`：IMU状态、四元数、加速度、角速度  
  - `SysStatusMsg`：系统电源、充电状态、电池电量

所有消息均包含统一的 `Header` 字段，方便 MQTT 转发和多设备管理。

### 目录结构

```
bionic_cat_mqtt_msgs/
├── include/
│   └── bionic_cat_mqtt_msg.hpp   # 核心消息定义头文件
├── src/
│   └── mqtt_msgs.cpp             # 消息实现（如有）
├── CMakeLists.txt                # 构建脚本
└── README.md                     # 项目说明
```

### 使用方法

1. 在你的模块 CMakeLists.txt 中添加依赖：
   ```cmake
   target_include_directories(<your_target> PRIVATE ${BIONIC_CAT_MQTT_MSGS_INCLUDE_DIRS})
   target_link_libraries(<your_target> PRIVATE ${BIONIC_CAT_MQTT_MSGS_LIBRARIES})
   ```
2. 在代码中包含头文件并使用相关消息结构体：
   ```cpp
   #include "bionic_cat_mqtt_msg.hpp"
   ```

### 适用场景

- 设备与云端、设备与设备之间的 MQTT 通信
- 机器人控制、状态同步、数据采集等场景

### 维护说明

如需新增消息类型，请在 `include/bionic_cat_mqtt_msg.hpp` 中补充定义。

---

