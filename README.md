# 全链路工业设备状态监控仿真系统

纯软件搭建完整工业物联网链路：**PLC(PLCSIM) → 上位机 → 后端 → 数据库 → Web大屏**，零硬件成本。

## 五层架构

```
┌──────────┐    S7:102    ┌──────────┐   HTTP POST   ┌──────────────┐   内嵌H2
│ S7-1200  │───NetToPLCsim→│ C# 上位机 │──────────────→│ SpringBoot   │──────────┐
│ (PLCSIM) │              │ (S7.NET) │  /api/device  │   :8081      │   ┌──────┴──────┐
└──────────┘              └──────────┘     -data      └──────┬───────┘   │  H2 Database │
                                                      │                   │  (文件存储)  │
                                                      │ GET /api/devices   └─────────────┘
                                                      ↓
                                          ┌──────────────┐       ┌──────────────┐
                                          │ 大屏 ECharts │       │ Flask :5000 │
                                          │ dashboard.html│←─────│ IMOGJO调度   │
                                          └──────────────┘       └──────────────┘
```

| 层 | 技术 | 端口 | 说明 |
|----|------|------|------|
| PLC仿真 | S7-1200 + PLCSIM + NetToPLCsim | 102 | 虚拟PLC，SCL程序控制6台设备 |
| 上位机 | C# WinForm + S7.NET | — | 2秒轮询，仿真/真实双模式 |
| 后端 | SpringBoot 3.5 + MyBatis + H2 | 8081 | REST API，OEE计算，Excel导出，内嵌数据库 |
| 算法 | Flask + IMOGJO | 5000 | 多目标金豺优化，故障设备自动剔除 |
| 大屏 | HTML + ECharts | — | 4 Tab：监控/OEE/调度/系统控制 |

## 端口清单

| 端口 | 服务 | 协议 | 说明 |
|------|------|------|------|
| 102 | NetToPLCsim | S7/TCP | PLC通信端口，NetToPLCsim绑定192.168.1.13 |
| 5000 | Flask | HTTP | IMOGJO调度算法微服务 |
| 8081 | SpringBoot | HTTP | 后端API + 大屏静态资源 + H2数据库 |

## 快速演示

1. 双击 `start.bat` 启动全部服务
2. 等待约15秒，浏览器自动打开监控大屏

默认使用仿真模式 + H2 内嵌数据库，零依赖，双击即跑。
如需 MySQL：参考下方"数据库切换"。

## 分步启动

### 前置依赖
- .NET SDK 10.0+
- Java 17+
- Python 3.10+，依赖：`pip install flask flask-cors numpy requests`

### 1. 启动 PLC 仿真环境
1. 以管理员身份运行 `01_PLC工程/NetToPLCsim.exe`
2. 在 NetToPLCsim 中点击 **Add** 添加站点
3. 配置：IP=`192.168.1.13`，PLC类型=S7-1200
4. 点击 **Start Server**，确认状态栏显示 "Server running"
5. 打开 TIA Portal → 启动 PLCSIM → 下载程序 → CPU 切 **RUN**
6. 验证：`netstat -ano | findstr ":102 "` 应显示 LISTENING

### 2. 启动后端服务
```bash
# 启动 SpringBoot (端口 8081)
cd monitor_server/demo && mvnw spring-boot:run

# 启动 Flask (端口 5000)
cd 05_算法服务 && python scheduler.py
```

### 3. 启动上位机
```bash
cd 02_上位机/DeviceHMI && dotnet run
```
HMI 窗口出现后，底部状态栏应显示"已连接 PLC S7-1200"，设备面板数据每2秒刷新。

### 4. 打开大屏
浏览器访问 `http://localhost:8081/dashboard.html`

### 一键启动
```bash
python launcher.py          # 交互菜单
python launcher.py start    # 直接启动全部
```
或双击 `start.bat`

## 项目结构

```
设备监控系统/
├── 01_PLC工程/          # SCL源码 + TIA Portal工程 + NetToPLCsim
├── 02_上位机/DeviceHMI/  # C# WinForm (.NET 9)
├── 03_后端/sql/          # 建库建表脚本
├── 04_大屏/              # 大屏HTML (CDN ECharts)
├── 05_算法服务/          # Flask IMOGJO调度算法
├── 06_演示截图/          # 演示截图与导出示例
├── monitor_server/demo/  # SpringBoot后端
├── docs/                # 开发文档
├── init_db.bat           # 数据库初始化
├── start.bat             # 一键启动
├── 快速开始.txt          # 极简启动指南
├── launcher.py           # 全服务状态管理
├── README.md
└── LICENSE
```

## 常见问题排查

### PLC 连不上 (HMI显示"未连接")
1. 检查 NetToPLCsim 是否 Start Server：`netstat -ano | findstr ":102 "` 无 LISTENING → 回到 NetToPLCsim 点击 Start Server
2. 检查 IP 是否绑定：必须确认 WiFi 网卡 IP 为 `192.168.1.13`
3. 检查 PLCSIM 是否 RUN 模式：PLCSIM 面板顶部状态灯应为绿色
4. 检查 TIA Portal CPU 属性：**保护 → 勾选"允许来自远程伙伴的 PUT/GET 通信访问"**（S7-1200 默认禁用）
5. 检查 DB1 属性：**"优化的块访问"必须取消勾选**（S7-1200 默认启用）

### 端口冲突
```bash
# 查看端口占用
netstat -ano | findstr ":8081"

# 结束占用进程
taskkill /PID <PID> /F
```

### 大屏数据不刷新
1. 确认后端在运行：`curl http://localhost:8081/api/devices` 应返回 JSON
2. 确认 HMI 在 POST 数据：HMI 日志应显示 `HTTP POST OK`
3. 打开浏览器 F12 控制台查看 JS 报错

### SpringBoot 启动失败
1. 确认 Java 17+ 已安装：`java -version`
2. 如端口被占用：`netstat -ano | findstr ":8081"`
3. H2 数据库文件自动创建于 `monitor_server/demo/data/`，删除该目录可重置数据

### Flask 连接后端失败
- Flask 通过 `http://localhost:8081/api/devices` 获取设备状态
- 确保 SpringBoot 先启动，再启动 Flask
- 如果后端重启过，Flask 无需重启（每次请求都会重新 GET）

## 仿真/真实PLC切换

编辑 `02_上位机/DeviceHMI/mode.txt`：
```
sim   ← 仿真模式（默认，无需 PLC，数据本地生成）
plc   ← 真实 PLC 模式（需 PLCSIM + NetToPLCsim + S7-1200）
```
改完重启 HMI 即时生效，无需重编译。

## 数据库切换

默认使用 **H2 内嵌数据库**，零安装零配置，数据存于 `monitor_server/demo/data/`。

切换到 **MySQL**：
1. 安装 MySQL 8.0，执行 `init_db.bat` 初始化
2. 启动 SpringBoot 时加 profile：
   ```bash
   cd monitor_server/demo && mvnw spring-boot:run -Dspring-boot.run.profiles=mysql
   ```
3. 或修改 `start.bat`，取消 MySQL 那行的注释，注释掉 H2 那行
