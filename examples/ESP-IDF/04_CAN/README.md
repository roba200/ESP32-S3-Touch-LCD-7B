| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 04_CAN

## 功能说明

本示例演示 CAN 通信收发。程序会先通过 CH422G 将接口切换到 CAN 模式，然后以 `500 kbps` 初始化 TWAI 控制器。收到 CAN 帧后，会将该帧再次发出。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 已连接 CAN 总线或 CAN 分析仪
- 总线已正确终端匹配

示例在程序启动时将 `IO_EXTENSION_IO_5` 置高，用于选择 CAN 通道。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 程序进入 CAN 监听状态
- 向开发板发送 CAN 数据帧后，开发板会将收到的帧重新发出

## 说明

- 本示例使用正常模式，不是内部回环模式
- 需要至少一个外部 CAN 节点参与测试

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
