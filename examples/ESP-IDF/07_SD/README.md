| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 07_SD

## 功能说明

本示例演示 Micro SD 卡挂载、容量读取和卸载流程。程序在串口输出存储卡信息，并在 LCD 上显示总容量和可用容量。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 已插入可正常识别的 Micro SD 卡

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- LCD 显示 `SD TEST`
- 挂载成功时，屏幕显示 `SD Card OK!`，并显示总容量与可用容量
- 串口输出 SD 卡参数，随后卸载文件系统
- 挂载失败时，屏幕显示 `SD Card Fail!`

## 说明

- 本示例在读取信息后会主动卸载文件系统
- 建议使用已格式化的存储卡进行测试

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
