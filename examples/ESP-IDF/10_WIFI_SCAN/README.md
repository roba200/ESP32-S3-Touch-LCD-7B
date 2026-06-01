| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 10_WIFI_SCAN

## 功能说明

本示例扫描周围的 Wi-Fi 接入点，并将扫描到的 SSID 显示在 LCD 右侧区域。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 测试环境中存在可扫描到的 2.4 GHz Wi-Fi 网络

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 屏幕左侧显示示例标题
- 屏幕右侧先显示 `Scanning now...`
- 扫描完成后，在右侧列表中显示扫描到的 SSID

## 说明

- 当前实现会跳过包含非 ASCII 字符的 SSID，因此中文名称不会显示在列表中
- 扫描结果数量受 `DEFAULT_SCAN_LIST_SIZE` 限制

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
