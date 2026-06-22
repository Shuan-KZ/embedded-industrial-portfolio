# 调试日志 — 工业设备监控系统

记录项目开发过程中实际遇到的技术问题、排查过程和解决方案。

## 1. S7 通信排坑

### 现象
HMI 连接 PLC 后 ReadAll 失败：
```
[PLC] ReadAll exception: Received 12 bytes: '32-02-00-00-00-00-00-00-00-00-81-04', expected 126 bytes.
```

### 分析
- 连接握手成功（COTP CC 响应正常，S7 Setup Communication 返回 27 字节确认）
- Read 请求发出后，收到了 S7 协议的 12 字节错误响应
- `32` = S7 协议标识，`81-04` = 错误码（无法访问对象）
- PLC 收到了读请求，但返回了错误而非数据

### 根因
S7-1200 默认禁用 PUT/GET 通信访问。S7.NET 库通过 PUT/GET 协议读取 DB 数据，禁用后 ReadBytes() 返回错误。

### 解决
TIA Portal → CPU 属性 → 保护 → **勾选"允许来自远程伙伴的 PUT/GET 通信访问"** → 重新编译下载

### 相关知识点
- S7-1200/1500 从固件 V4.x 开始默认禁用 PUT/GET
- S7-300/400 无此限制（默认启用）
- 如果 CPU 有密码保护，PUT/GET 可能需要额外授权

## 2. NetToPLCsim 通信排坑

### 现象
HMI 日志持续输出：
```
[PLC] Connect failed: 目标计算机积极拒绝，无法连接。 [::ffff:192.168.1.13]:102
```

### 排查步骤
1. 检查 NetToPLCsim 进程：`tasklist | findstr NetToPLCsim` → 进程存在（PID 16172）
2. 检查端口监听：`netstat -ano | findstr ":102 "` → **无 LISTENING 状态**
3. 结论：NetToPLCsim 进程在运行，但未配置站点/未启动 Server

### 根因
NetToPLCsim 启动后不会自动开始监听。需要手动：
1. 添加站点（Add）
2. 配置 IP 和 PLC 类型
3. 点击 Start Server

### 解决
1. 切换到 NetToPLCsim 窗口
2. 确认或添加站点：IP=`192.168.1.13`，类型=S7-1200
3. 点击 **Start Server**
4. 验证：`netstat -ano | findstr ":102 "` 出现 `LISTENING`

### 注意
- NetToPLCsim 以管理员身份运行才能绑定 102 端口（<1024 端口需要管理员权限）
- WiFi 网卡 IP 必须为 `192.168.1.13`，否则 NetToPLCsim 无法绑定该 IP
- 如果 IP 不对，修改 WiFi 适配器 IPv4 地址为 `192.168.1.13`

## 3. PLC 数据块配置排坑

### 现象
HMI ReadAll 返回数据全是 0 或乱码。

### 根因
S7-1200 默认使用"优化块访问"（Optimized Block Access），数据布局由固件管理，按符号寻址。S7.NET 库需要"非优化块访问"，按绝对偏移寻址。

### 解决
TIA Portal → 右键 DB1 → 属性 → **取消勾选"优化的块访问"**（`S7_Optimized_Access := 'FALSE'`）

### 验证
- 优化块：`DB1.scl` 中 `S7_Optimized_Access := 'TRUE'`（或省略此行，默认 TRUE）
- 非优化块：`S7_Optimized_Access := 'FALSE'`
- DB1 改为非优化后需重新编译下载

### DB1 字节布局
```
每设备 18 字节，6 设备 = 108 字节
偏移  0-3:  温度 REAL (4B)
偏移  4-7:  振动 REAL (4B)
偏移  8-11: 压力 REAL (4B)
偏移 12-13: 状态 INT  (2B)
偏移 14-17: 能耗 REAL (4B)
```

## 4. 端口冲突解决

### 8081 端口被占用
```bash
# 查看占用进程
netstat -ano | findstr ":8081"
# TCP    0.0.0.0:8081    0.0.0.0:0    LISTENING    14744

# 强制结束
taskkill /PID 14744 /F
```

### 5000 端口被占用
```bash
netstat -ano | findstr ":5000"
taskkill /PID <PID> /F
```

### 102 端口被占用
通常由上次残留的 NetToPLCsim 实例引起：
```bash
taskkill /F /IM NetToPLCsim.exe
```
然后重新以管理员身份启动。

## 5. H2 数据库

H2 内嵌数据库，数据文件存储于 `monitor_server/demo/data/` 目录。
删除该目录即可重置所有数据，下次启动自动重建。

H2 控制台：`http://localhost:8081/h2-console`
JDBC URL: `jdbc:h2:file:./data/device_monitor`  用户名: `sa`  密码: 空

## 6. HMI 调试日志位置

HMI 运行时自动输出日志到：
- 文件：`%TEMP%\hmi_debug.log`（系统临时目录）
- 控制台：`dotnet run` 的输出窗口

日志格式：`[HH:mm:ss.fff] [模块] 消息`

## 7. 快速健康检查命令

```bash
# 全端口检查
netstat -ano | findstr /R ":102.*LISTENING :5000.*LISTENING :8081.*LISTENING"

# 全进程检查
tasklist | findstr /I "NetToPLCsim python java dotnet"

# SpringBoot API 检查
curl -s http://localhost:8081/api/devices | python -m json.tool

# Flask 健康检查
curl -s http://localhost:5000/api/health
```

## 通信拓扑图

```
192.168.1.13:102 (NetToPLCsim)
        ↕ S7 protocol
127.0.0.1:5000 (Flask) ←→ 127.0.0.1:8081 (SpringBoot)
                              ↕ JDBC
                              (H2内嵌)

上位机 (C#) → 192.168.1.13:102 (读PLC)
上位机 (C#) → 127.0.0.1:8081 (POST数据)
浏览器     → 127.0.0.1:8081 (大屏+API)
```

## 注意事项

1. **WiFi IP 必须是 192.168.1.13**，否则 NetToPLCsim 无法绑定
2. **NetToPLCsim 每次重启**后需要重新 Add 站点 + Start Server（无配置持久化）
3. **PLCSIM 每次下载**后需手动切 RUN
4. **DB1 改结构**后需重新编译下载，并更新 HMI 中的偏移量
5. **SpringBoot 首次启动**会在 `monitor_server/demo/data/` 自动创建 H2 数据库文件
