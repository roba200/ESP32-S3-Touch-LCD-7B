| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 06_LCD

## 功能说明

本示例演示 RGB LCD 的基础绘图流程。程序会依次显示颜色条、点线矩形圆形、英文与中文字符、位图和内置图片资源。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置

## 可修改项

- 屏幕旋转方向由 `main/main.c` 中的 `ROTATE` 宏控制，可选 `ROTATE_0`、`ROTATE_90`、`ROTATE_180`、`ROTATE_270`

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 屏幕背光点亮
- 依次显示颜色条测试图
- 依次显示图元、文本、位图和图片资源

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
