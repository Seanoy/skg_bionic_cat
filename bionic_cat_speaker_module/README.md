# bionic_cat_speaker_module

面向 CVITEK CV184X 平台的扬声器播放模块。模块作为 MQTT 订阅者接收上层指令（AudioPlayCommand），使用 tinyalsa 播放本地 WAV 音频文件，可控制速度、音量和循环。

- 默认 MQTT 服务器：tcp://localhost:1883
- 默认订阅主题：bionic_cat/audio_play_command
- 默认 QoS：1


## 功能特性
- 订阅并解析二进制消息 AudioPlayCommand（使用仓库内 serializer.hpp）
- 本地 WAV 播放（8/16/24/32-bit PCM，LE）
- 播放控制：速度、音量、循环开关
- 简单且稳定：独立可执行进程，收/播解耦


## 目录结构
- include/
  - speaker_node.hpp：MQTT 订阅、消息分发与播放器控制
  - play_wav_tinyalsa.hpp：基于 tinyalsa 的 WAV 播放器
- src/
  - main.cpp：入口与常量配置（服务器、主题、QoS）
  - speaker_node.cpp：订阅与命令处理
  - play_wav_tinyalsa.cpp：WAV 解析与播放实现
- CMakeLists.txt：模块构建脚本
- README.md：本说明


## 依赖与前置条件
- 运行平台：CV184X SoC（A53 Linux）
- 上游依赖（由顶层工程统一查找/链接）：
  - paho_mqtt_cpp::paho_mqtt_cpp、paho_mqtt_c::paho_mqtt_c
  - tinyalsa::tinyalsa
  - tdl_core 及相关中间件（由顶层 CMake 注入）
- MQTT Broker：建议 mosquitto，默认监听 1883
- 音频文件：本地可访问的 WAV（PCM，小端），路径由消息指定


## 构建与安装
本模块由仓库顶层 CMake 一起构建（已在顶层 CMakeLists.txt 中 add_subdirectory）。

1) 生成与配置（示例）：
- cmake -S . -B build -DUSE_CROSS_COMPLIER=ON

2) 编译：
- cmake --build build -j

3) 安装（可选）：
- cmake --install build --prefix install

安装后可执行文件将位于 install/bionic_cat/ 下。


## 运行
可执行文件：speaker_module（无命令行参数）

- ./speaker_module

运行时将连接 MQTT 并订阅 bionic_cat/audio_play_command。按 Ctrl+C 可安全退出。

如需修改服务器地址、主题或 ClientId，请编辑 src/main.cpp 中常量后重新编译：
- SERVER_ADDRESS（默认 tcp://localhost:1883）
- SUBSCRIBE_TOPIC（默认 bionic_cat/audio_play_command）
- CLIENT_ID（默认 speaker_node）


## MQTT 指令与消息格式
- 主题：bionic_cat/audio_play_command
- 负载：二进制序列化（使用 bionic_cat_mqtt_utils/serializer.hpp）
- 类型：BionicCat::MqttMsgs::AudioPlayCommand

字段说明：
- file_path: string，WAV 文件绝对或相对路径
- speed: float，播放速度，1.0 原速，>1 加速，<1 减速（简单采样法）
- volume: float，音量，0.0~2.0（1.0 为原始幅度）
- loop: bool，是否循环播放

注意：请勿发送 JSON 文本。必须使用相同的 Serializer 将结构体编码为二进制后发布。


## 音频支持与限制
- 支持 WAV PCM（8/16/24/32-bit，LE），采样率与通道数按文件原始信息输出
- 速度调节为轻量实现：
  - 加速：降采样式抽取（间隔取样），可能产生伪影
  - 减速：样本重复，音质一般
- 音量为样本幅度线性缩放，可能裁剪（clamp）
- 使用默认声卡/设备（card=0, device=0）。如需切换声卡，需扩展代码（见“配置与扩展”）


## 配置与扩展建议
- 切换 MQTT 服务器或主题：修改 src/main.cpp 常量重新编译
- 从消息携带设备信息：可在 AudioPlayCommand 或其 header 中扩展 card/device 字段
- 支持更多格式：可接入 libsox/ffmpeg 做解码后走 tinyalsa 播放
- 更高音质变速：集成高质量重采样或 WSOLA/PhaseVocoder 等算法


## 故障排查
- 无法连接 MQTT：
  - 确认 Broker 已启动并地址/端口正确；网络可达
- 收到指令但无声音：
  - 检查 file_path 是否存在且可读；确认 WAV 为 PCM LE
  - 检查声卡：aplay -l；确认 card/device 正确且未被占用
  - ALSA 权限：确保当前用户对音频设备有访问权限
- 声音失真或速度异常：
  - speed 调整为 1.0 试验；降低 volume；确认位宽与采样率
- 进程退出：
  - 检查日志；若反复断开，查看 Broker 稳定性与网络


## 开发者速览
- main.cpp：注册信号 -> 创建 SpeakerNode -> init() 连接与订阅 -> run()
- SpeakerNode：收到消息 -> 反序列化 AudioPlayCommand -> 配置/启动 WavPlayer
- WavPlayer：解析 WAV 头 -> 打开 PCM -> 读取块并进行速度/音量处理 -> 写入 tinyalsa


## 许可证
遵循仓库根目录 LICENSE。


## 致谢
- Paho MQTT
- tinyalsa
