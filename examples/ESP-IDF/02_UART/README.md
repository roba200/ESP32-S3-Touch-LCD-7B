| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 02_UART

## 功能说明

本示例演示 UART 回环。程序从 `GPIO44` 接收串口数据，再通过 `GPIO43` 原样发回。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 准备 USB 转串口模块或其他 UART 设备

## 接线说明

请将外部串口设备与开发板交叉连接：

- 开发板 `GPIO43` 连接外部设备 RX
- 开发板 `GPIO44` 连接外部设备 TX
- 开发板 GND 与外部设备 GND 共地

默认波特率为 `115200`。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 向 `GPIO44` 对应的串口输入任意数据
- 程序会通过 `GPIO43` 回发收到的内容

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
