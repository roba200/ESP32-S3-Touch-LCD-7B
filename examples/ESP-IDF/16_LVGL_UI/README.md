| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# 16_LVGL_UI

## 功能说明

本示例提供一个基于 LVGL 的综合界面，包含以下功能模块：

- 登录与创建用户，账号信息保存在 NVS
- Wi-Fi 扫描、连接和 SoftAP 管理
- RS485 收发界面
- CAN 收发界面
- PWM 背光调节与电池电压显示
- Micro SD 卡相关显示功能

界面资源位于 `main/ui/`，业务逻辑位于 `main/user/`。

## 前置条件

- 开发板：微雪 `ESP32-S3-Touch-LCD-7B`
- 已完成 ESP-IDF 开发环境配置
- 若需测试 RS485、CAN、Wi-Fi 或 SD 功能，请提前准备对应外设或网络环境

## 构建与烧录

1. 在当前目录执行 `idf.py set-target esp32s3`
2. 执行 `idf.py -p PORT flash monitor`

## 运行现象

- 启动后进入登录界面
- 可创建账户并保存到 NVS，重新上电后仍可使用
- 登录后可通过触摸屏进入 Wi-Fi、RS485、CAN 和 PWM 等子界面
- Wi-Fi 功能由后台任务初始化并处理

## 说明

- 该示例依赖触摸屏完成主要交互
- 如果需要调整 UI 布局，可修改 `main/ui/` 下的界面文件
- 如果需要调整业务行为，可修改 `main/user/` 下的对应模块

## 参考

- ESP-IDF 入门说明：<https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/>
- LVGL 文档：<https://docs.lvgl.io/>
