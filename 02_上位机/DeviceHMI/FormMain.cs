using System.Timers;

namespace DeviceHMI;

public class FormMain : Form
{
    private readonly PLCService _plc = new();
    private readonly System.Timers.Timer _timer = new(2000);
    private int _failCount;

    // 每台设备的UI控件
    private readonly TextBox[] _txtTemp = new TextBox[6];
    private readonly TextBox[] _txtVib = new TextBox[6];
    private readonly TextBox[] _txtPress = new TextBox[6];
    private readonly TextBox[] _txtEnergy = new TextBox[6];
    private readonly Panel[] _pnlStatus = new Panel[6];
    private readonly int[] _lastStatus = new int[6];
    private readonly ToolStripStatusLabel _lblConn;
    private readonly RichTextBox _rtbLog;

    public FormMain()
    {
        Text = "工业设备监控上位机";
        Size = new Size(1300, 800);
        BackColor = Color.FromArgb(18, 18, 46);
        StartPosition = FormStartPosition.CenterScreen;
        Font = new Font("Microsoft YaHei", 9F);

        // === 顶部标题栏 ===
        var titleBar = new Panel
        {
            Height = 48, Dock = DockStyle.Top,
            BackColor = Color.FromArgb(12, 12, 36)
        };
        var title = new Label
        {
            Text = "工业设备监控上位机",
            ForeColor = Color.FromArgb(56, 184, 196),
            Font = new Font("Microsoft YaHei", 14F, FontStyle.Regular),
            Location = new Point(20, 12), AutoSize = true
        };
        titleBar.Controls.Add(title);
        Controls.Add(titleBar);

        // === 6设备面板 3x2 ===
        var grid = new TableLayoutPanel
        {
            ColumnCount = 3, RowCount = 2,
            Dock = DockStyle.Fill,
            Padding = new Padding(12, 8, 12, 8),
            CellBorderStyle = TableLayoutPanelCellBorderStyle.None
        };
        for (int c = 0; c < 3; c++) grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.3F));
        for (int r = 0; r < 2; r++) grid.RowStyles.Add(new RowStyle(SizeType.Percent, 50F));

        for (int i = 0; i < 6; i++)
        {
            var dev = _plc.Devices[i];
            var gb = new GroupBox
            {
                Text = dev.Name,
                ForeColor = Color.White,
                Font = new Font("Microsoft YaHei", 10F, FontStyle.Bold),
                Margin = new Padding(6),
                Padding = new Padding(10, 20, 10, 10)
            };
            BuildDevicePanel(gb, i);
            grid.Controls.Add(gb, i % 3, i / 3);
        }
        Controls.Add(grid);

        // === 底部区域 ===
        var bottom = new Panel { Height = 160, Dock = DockStyle.Bottom, Padding = new Padding(12, 4, 12, 8) };

        // 连接状态
        var statusStrip = new StatusStrip
        {
            BackColor = Color.FromArgb(12, 12, 36),
            ForeColor = Color.FromArgb(90, 120, 150),
            Height = 24
        };
        _lblConn = new ToolStripStatusLabel { Text = "未连接", ForeColor = Color.FromArgb(90, 120, 150) };
        statusStrip.Items.Add(_lblConn);
        bottom.Controls.Add(statusStrip);

        // 日志区
        var logLabel = new Label
        {
            Text = "事件日志:", ForeColor = Color.FromArgb(90, 120, 150),
            Location = new Point(12, 26), AutoSize = true, Font = new Font("Microsoft YaHei", 8F)
        };
        bottom.Controls.Add(logLabel);

        _rtbLog = new RichTextBox
        {
            Location = new Point(12, 44), Size = new Size(1260, 105),
            BackColor = Color.FromArgb(5, 5, 20), ForeColor = Color.LimeGreen,
            ReadOnly = true, BorderStyle = BorderStyle.None,
            Font = new Font("Consolas", 8.5F)
        };
        bottom.Controls.Add(_rtbLog);
        Controls.Add(bottom);

        // Timer
        _timer.Elapsed += TimerTick;
        _timer.AutoReset = true;
        Load += (_, _) => OnLoad();
        FormClosing += (_, _) => { _timer.Stop(); _plc.Disconnect(); };
    }

    private void BuildDevicePanel(GroupBox gb, int idx)
    {
        int left = 12, top = 32, rowH = 32;

        AddLabel(gb, "温度:", left, top);
        _txtTemp[idx] = AddValueBox(gb, left + 65, top - 4);
        AddLabel(gb, "振动:", left, top + rowH);
        _txtVib[idx] = AddValueBox(gb, left + 65, top + rowH - 4);
        AddLabel(gb, "压力:", left, top + rowH * 2);
        _txtPress[idx] = AddValueBox(gb, left + 65, top + rowH * 2 - 4);
        AddLabel(gb, "能耗:", left, top + rowH * 3);
        _txtEnergy[idx] = AddValueBox(gb, left + 65, top + rowH * 3 - 4);

        // 状态灯
        _pnlStatus[idx] = new Panel
        {
            Size = new Size(40, 40),
            Location = new Point(265, top),
            BackColor = Color.Gray
        };
        gb.Controls.Add(_pnlStatus[idx]);

        // 启停按钮
        int btnTop = top + rowH * 2 + 40;
        var btnStart = new Button
        {
            Text = "启动", Location = new Point(left, btnTop),
            Size = new Size(70, 28), FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(70, 194, 136), ForeColor = Color.White,
            Font = new Font("Microsoft YaHei", 8F)
        };
        btnStart.FlatAppearance.BorderSize = 0;
        int devId = idx + 1;
        btnStart.Click += (_, _) => { _plc.SetDeviceRunning(devId, true); Log($"设备{devId} 启动指令已发送"); };
        gb.Controls.Add(btnStart);

        var btnStop = new Button
        {
            Text = "停止", Location = new Point(left + 80, btnTop),
            Size = new Size(70, 28), FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(224, 85, 106), ForeColor = Color.White,
            Font = new Font("Microsoft YaHei", 8F)
        };
        btnStop.FlatAppearance.BorderSize = 0;
        btnStop.Click += (_, _) => { _plc.SetDeviceRunning(devId, false); Log($"设备{devId} 停止指令已发送"); };
        gb.Controls.Add(btnStop);
    }

    private static Label AddLabel(Control parent, string text, int x, int y)
    {
        var lbl = new Label { Text = text, Location = new Point(x, y), AutoSize = true,
            ForeColor = Color.FromArgb(180, 200, 220), Font = new Font("Microsoft YaHei", 9F) };
        parent.Controls.Add(lbl);
        return lbl;
    }

    private static TextBox AddValueBox(Control parent, int x, int y)
    {
        var tb = new TextBox { Location = new Point(x, y), Size = new Size(80, 22),
            ReadOnly = true, BackColor = Color.FromArgb(10, 20, 40),
            ForeColor = Color.FromArgb(56, 184, 196), BorderStyle = BorderStyle.None,
            Font = new Font("Consolas", 10F, FontStyle.Bold) };
        parent.Controls.Add(tb);
        return tb;
    }

    private void OnLoad()
    {
        Log("上位机启动");
        FileLog.Write($"[HMI] OnLoad, connecting...");
        _plc.Connect();
        FileLog.Write($"[HMI] Connect: OK={_plc.Connected}, Status={_plc.ConnectionStatus}");
        _lblConn.Text = _plc.ConnectionStatus;
        _lblConn.ForeColor = _plc.Connected ? Color.FromArgb(70, 194, 136) : Color.FromArgb(216, 156, 104);
        _timer.Start();
        Log(_plc.ConnectionStatus);
    }

    private async void TimerTick(object? sender, ElapsedEventArgs e)
    {
        if (!_plc.Connected)
        {
            _plc.Connect();
            FileLog.Write($"[HMI] Reconnecting... Connected={_plc.Connected}");
            UpdateConnectionUI();
            return;
        }

        if (!_plc.ReadAll())
        {
            _failCount++;
            FileLog.Write($"[HMI] ReadAll failed, failCount={_failCount}");
            if (_failCount >= 3) UpdateConnectionUI();
            return;
        }

        _failCount = 0;
        FileLog.Write($"[HMI] ReadAll OK T1={_plc.Devices[0].Temperature:F1} V1={_plc.Devices[0].Vibration:F1}");
        UpdateConnectionUI();
        BeginInvoke(() => UpdateAllUI());
        await HttpHelper.SendDeviceDataAsync(_plc.Devices);
    }

    private void UpdateConnectionUI()
    {
        BeginInvoke(() => {
            _lblConn.Text = _plc.ConnectionStatus;
            _lblConn.ForeColor = _plc.Connected ? Color.FromArgb(70, 194, 136) : Color.FromArgb(224, 85, 106);
        });
    }

    private void UpdateAllUI()
    {
        for (int i = 0; i < 6; i++)
        {
            var d = _plc.Devices[i];
            _txtTemp[i].Text = $"{d.Temperature:F1} ℃";
            _txtVib[i].Text = $"{d.Vibration:F1} mm/s";
            _txtPress[i].Text = $"{d.Pressure:F2} MPa";
            _txtEnergy[i].Text = $"{d.CumulativeEnergy:F2} kWh";

            // 状态灯变色
            Color c = d.Status switch
            {
                2 => Color.FromArgb(224, 85, 106),  // 红色 故障
                1 => Color.FromArgb(216, 156, 104),  // 橙色 预警
                _ => Color.FromArgb(70, 194, 136)    // 绿色 正常
            };
            if (_pnlStatus[i].BackColor != c) _pnlStatus[i].BackColor = c;

            // 告警检测（状态变化时触发）
            if (d.Status != _lastStatus[i])
            {
                if (d.Status == 2)
                {
                    Log($"[故障] {d.Name} 温度过高: {d.Temperature}℃");
                    Task.Run(() => MessageBox.Show($"{d.Name} 故障告警!\n温度: {d.Temperature}℃", "故障告警",
                        MessageBoxButtons.OK, MessageBoxIcon.Warning));
                }
                else if (d.Status == 1 && _lastStatus[i] != 2)
                {
                    Log($"[预警] {d.Name} 温度偏高: {d.Temperature}℃");
                }
                else if (d.Status == 0 && _lastStatus[i] != 0)
                {
                    Log($"[恢复] {d.Name} 状态恢复正常");
                }
                _lastStatus[i] = d.Status;
            }
        }
    }

    private void Log(string msg)
    {
        BeginInvoke(() => {
            string time = DateTime.Now.ToString("HH:mm:ss");
            _rtbLog.AppendText($"[{time}] {msg}\n");
            if (_rtbLog.Lines.Length > 200)
            {
                _rtbLog.Select(0, _rtbLog.GetFirstCharIndexFromLine(Math.Max(0, _rtbLog.Lines.Length - 200)));
                _rtbLog.SelectedText = "";
            }
        });
    }
}
