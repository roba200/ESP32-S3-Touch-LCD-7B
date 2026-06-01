| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 05-RS485

## 功能说明

本示例演示 RS485 串口回环。程序从 RS485 接口接收数据，再将收到的内容原样发回。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 已连接 RS485 主机、USB 转 RS485 模块或其他 RS485 设备

默认使用 `GPIO16` 作为 TX、`GPIO15` 作为 RX，波特率为 `921600`。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 通过 RS485 接口发送任意数据
- 开发板会回发收到的内容

## 说明

- 测试时请确认外部设备的波特率、数据位和校验设置与示例一致
- 如果使用 USB 转 RS485 模块，请确认总线 A/B 连接正确

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
