| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 09_Display_bmp

## 功能说明

本示例从 Micro SD 卡中查找 `.bmp` 文件，并在 LCD 上显示。触摸屏幕中的左右箭头区域可切换上一张或下一张图片。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 已插入 Micro SD 卡
- 已将待显示的 `.bmp` 文件复制到存储卡挂载目录

仓库中的 `pic/` 目录提供了测试图片，可先复制到存储卡后再运行示例。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- SD 卡初始化成功后，屏幕提示可以开始切换图片
- 未找到 `.bmp` 文件时，屏幕显示对应提示
- 触摸左侧箭头区域显示上一张图片
- 触摸右侧箭头区域显示下一张图片

## 说明

- 程序只会枚举后缀名为 `.bmp` 的文件
- SD 卡初始化失败时，示例直接停止在错误提示界面

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
