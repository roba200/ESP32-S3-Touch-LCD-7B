| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 11_WIFI_STA

## 功能说明

本示例以 STA 模式连接指定的 Wi-Fi 接入点。连接成功后，LCD 会显示获取到的 IP 地址、SSID 和密码；连接失败时会显示失败提示。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 已准备可连接的 2.4 GHz Wi-Fi 网络

## 配置项

请先修改 `main/main.c` 中的以下宏：

- `USER_SSID`：目标 Wi-Fi 名称
- `USER_PASS`：目标 Wi-Fi 密码

当前示例调用 `wifi_sta_init(..., WIFI_AUTH_WPA2_PSK)`，默认按 WPA2-PSK 方式连接。

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 连接过程中，LCD 显示 `wifi connecting......`
- 连接成功后，屏幕显示 IP 地址、SSID 和密码
- 多次重试后仍失败时，屏幕显示连接失败提示

## 说明

- 示例会将密码直接显示在屏幕上，公开环境下测试时请留意信息暴露风险
- 如需更换认证方式，可继续调整 `wifi_sta_init()` 的参数

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
