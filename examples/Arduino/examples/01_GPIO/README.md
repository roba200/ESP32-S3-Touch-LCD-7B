# 01_GPIO

## 功能说明

本示例演示 Arduino 环境下的 GPIO 输出控制。程序会翻转板载 LED 的电平，使 LED 以 500 ms 的间隔闪烁。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 Arduino IDE 或 Arduino CLI 的 ESP32 开发环境配置

本示例使用板载 LED，对应引脚为 `GPIO6`，无需额外接线。

## 烧录方法

1. 在 Arduino IDE 中打开 `01_GPIO.ino`
2. 选择正确的 ESP32-S3 开发板型号与串口
3. 点击上传

## 运行现象

- 板载 LED 按 500 ms 的间隔持续闪烁
