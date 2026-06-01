| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 08_Touch

## 功能说明

本示例演示 GT911 触摸驱动与双缓冲显示。程序最多同时跟踪 5 个触点，并在屏幕上用不同颜色的圆形标出各触点位置。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 屏幕初始化后显示白色背景
- 手指触摸屏幕时，会在对应位置显示彩色圆点
- 最多支持 5 点同时触摸

## 说明

- 本示例通过双缓冲降低撕裂现象
- 触点抬起后，对应标记会被清除

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
