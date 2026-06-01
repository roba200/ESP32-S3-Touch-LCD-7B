| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 01_GPIO

## 功能说明

本示例演示 GPIO 输出控制。程序会翻转板载 LED 的电平，使 LED 以 500 ms 的间隔闪烁。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置

本示例使用板载 LED，对应引脚为 `GPIO6`，无需额外接线。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

首次编译会下载并构建依赖，耗时通常较长。

## 运行现象

- 板载 LED 按 500 ms 的间隔持续闪烁
- 串口无额外交互要求

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
