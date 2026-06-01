| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 14_LVGL_BTN

## 功能说明

本示例使用 LVGL 创建一个按钮。点击屏幕中的按钮后，程序会翻转板载 LED 的状态。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置

板载 LED 使用 `GPIO6`。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 屏幕中央显示一个名为 `Button` 的 LVGL 按钮
- 触摸该按钮时，板载 LED 在亮和灭之间切换

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
- LVGL 文档：<https://docs.lvgl.io/>
