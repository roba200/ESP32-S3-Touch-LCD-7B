| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 15_LVGL_SLIDER

## 功能说明

本示例使用 LVGL 创建滑块控件，通过 PWM 调节板载 LED 亮度，并在界面上显示电池电压。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置

板载 LED 使用 `GPIO6`，电池电压通过 I/O 扩展芯片的 ADC 接口读取。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 屏幕中央显示亮度滑块
- 拖动滑块时，LED 亮度随之变化
- 界面下方持续显示 `BAT:x.xxV` 电池电压信息

## 说明

- 当前电路中的 LED 为低电平点亮，因此代码中对 PWM 占空比做了反向处理
- 示例启动后会周期性读取 ADC 并刷新电池电压标签

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
- LVGL 文档：<https://docs.lvgl.io/>
