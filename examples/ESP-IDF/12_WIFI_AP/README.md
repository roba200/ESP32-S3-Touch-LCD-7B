| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 12_WIFI_AP

## 功能说明

本示例将开发板配置为 SoftAP。设备接入后，LCD 会显示当前连接数量以及各终端的 MAC 地址。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 具备可连接该热点的手机或电脑

## 配置项

请按需修改 `main/main.c` 中的以下宏：

- `USER_SSID`：热点名称，默认值为 `ESP32-S3-Touch-LCD-7B`
- `USER_PASS`：热点密码，默认值为 `66668888`

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 启动后，开发板创建一个 Wi-Fi 热点
- LCD 初始显示 `Connected: 0`
- 有终端接入或断开时，屏幕会刷新连接数量与已连接终端的 MAC 地址

## 说明

- 当前配置最多允许 5 个终端连接
- 当密码为空字符串时，热点会按开放网络方式启动

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
