| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 03_I2C

## 功能说明

本示例演示开发板内部 I2C 外设访问流程。程序初始化 I2C 总线和 CH422G I/O 扩展芯片，并通过 `IO_EXTENSION_IO_2` 周期性开关 LCD 背光。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置

示例使用板载 I2C 设备，无需外接传感器。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- LCD 背光按 500 ms 的间隔亮灭切换
- 该过程用于验证 I2C 总线与 I/O 扩展芯片通信正常

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
