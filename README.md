## 项目概述

名称: GUI_CLIENT_G — 驾校学车系统设备端（车载终端）

目标平台: Raspberry Pi / Linux（基于 Qt Quick 的桌面/触摸界面）

功能要点:
- 读取 MFRC522 RFID 卡并记录学时（上车签到 / 下车签退）
- 与管理端通过 TCP 长连接交换 JSON 消息
- 离线缓存：断网时本地缓存发送数据，网络恢复后自动补传
- 支持发卡注册模式、科目选择与理论练习答题模块
- 使用 Qt (QML) 实现 UI，C++ 负责硬件与网络逻辑

## 通信协议 (示例)

1) 正常刷卡（上车签到）

{
  "Action": "上车签到",
  "CardID": "69AB2A07",
  "Duration": 0,
  "device_id": "100000005B105A9F",
  "sign": "b72a30465e32fe71344505dcb15d51c5",
  "timestamp": "1773987562",
  "type": "card"
}

2) 心跳（练习过程中周期性发送）

{
  "CardID": "69AB2A07",
  "device_id": "100000005B105A9F",
  "sign": "c453a9d5810302b17007b07e060447c8",
  "timestamp": "1773987581",
  "type": "heartbeat"
}

3) 下车签退

{
  "Action": "下车签退",
  "CardID": "B3D10F07",
  "Duration": 158,
  "device_id": "100000005B105A9F",
  "sign": "2ca6b3022a27ac4dbd67c668349c8bcf",
  "timestamp": "1773990677",
  "type": "card"
}

另外程序还使用 `type` 为 `theory`（理论成绩），`issue_card`（发卡）等辅助类型。后端返回的应答也包含 `status` 与 `type` 字段（如 `theory_ok`, `invalid`, `issue_reply` 等）。

## 安全签名说明

每条上报的 JSON 包都包含 `timestamp` 和 `sign` 字段。签名由 `cardId + type + timestamp + subject + secretSalt` 的 MD5（Qt 的 QCryptographicHash）计算得到。
签名盐值在代码中定义为 `devicebackend.cpp` 内的 `secretKey`，请在生产环境中替换为安全的密钥管理方式。

## 架构与主要文件

- `main.cpp`：应用入口，注册 QML 类型并加载 `main.qml`。
- `devicebackend.*`：QML 与底层线程之间的桥接，生成签名，负责构造发送的 JSON。
- `rfidthread.*`：硬件轮询与网络线程，负责 MFRC522 读卡、TCP 连接、离线缓存与补发逻辑。
- `MFRC522.*`：RFID 驱动（基于 bcm2835 / SPI，已适配树莓派）。
- `theorymanager.*`：题库加载、答题逻辑与结果导出（供 QML 调用）。
- `main.qml`、`SubjectOverlay.qml`：界面与交互逻辑。
- `questions_1.json`、`questions_4.json`：科目一、科目四题库示例。

## 离线缓存与补传

当网络不可用时，`RfidThread::sendJson` 会将发送失败的 JSON 字符串加入内存缓存 `r_offlineCache`，并通过 `cacheSizeChanged` 通知 UI。连接恢复时，会按顺序补发缓存中的所有数据并清空缓存（补发时有短延时以减少 TCP 粘包）。

注意：当前实现的离线缓存保存在内存（vector）中；若需要重启后持久化，请修改为写入文件并在重启时恢复。

## 配置项（需修改源码并重新编译）

- 服务器 IP 与端口：在 `rfidthread.cpp` 中硬编码为 `server_ip = "10.129.101.194"` 和 `port = 8888`。运行前请根据管理端地址调整。
- 签名盐值：`devicebackend.cpp` 中的 `secretKey`，请替换为安全密钥。

## 构建与运行（Raspberry Pi）

依赖项（示例）:
- Qt 5（Qt Quick、Qt QML、Qt Quick Controls）
- bcm2835 库（用于 SPI 与 GPIO）
- 编译工具链：g++, make, qmake

在树莓派上大致步骤：

```bash
# 安装依赖（示例）
sudo apt update
sudo apt install build-essential qt5-qmake qt5-default libqt5qml5 libqt5quickcontrols2-5-dev libqt5webkit5-dev
# 安装 bcm2835（如果没有）
# 下载并安装 https://www.airspayce.com/mikem/bcm2835/

# 使用 qmake 构建
qmake terminal.pro
make -j4
```

运行时注意：`MFRC522` 驱动使用 bcm2835，需要 root 权限 操作 SPI/GPIO，建议以 `sudo` 运行二进制：

```bash
sudo ./GUI_CLIENT_G  # 或生成的可执行文件名
```

如果在没有硬件或在开发机上运行，可注释掉 `MFRC522` 的初始化或改为提供模拟类以避免对 `bcm2835_init()` 的依赖。

## 使用说明（UI）

- 启动应用后会在顶部显示设备号（从 `/proc/cpuinfo` 中读取 Serial）。若无法读取则显示 `DEV-offline-mode`。
- 空闲时通过覆盖层选择练习科目（`SubjectOverlay.qml`）并加载对应题库。
- 刷卡流程：检测到卡后，底层发送 `card` 类型 JSON 到服务器；服务端回复 `showCardInfo` 所需信息，QML 根据回复切换状态（上车/下车/非法等）。
- 发卡注册模式：通过服务端下发 `control` 类型命令（`enter_issue_mode` / `exit_issue_mode`）实现。发卡时会以 `issue_card` 类型发往服务端。

## 题库

题库文件保存在程序运行目录下（示例：`questions_1.json`, `questions_4.json`）。格式为 JSON 数组，每项包含 `question`, `options`, `answer` 等字段。

## 常见调试点

- 若无法读卡：检查 SPI 与 MFRC522 接线、是否启用 SPI（raspi-config），并确保以 root 运行。
- 若无法连接管理端：检查 `rfidthread.cpp` 中的 IP/port，并确认管理端在对应地址监听。
- 若签名校验失败：确认设备与服务器使用相同的签名策略与盐值。

## 许可证与致谢

该工程整合了 MFRC522 驱动的开源实现（原作者信息见 `MFRC522.h` 文件头），界面基于 Qt Quick。请根据实际需求补充许可证信息。

---

如需把 `server_ip` 与 `port` 改为配置文件或命令行参数，可以继续实现并更新构建说明。
    