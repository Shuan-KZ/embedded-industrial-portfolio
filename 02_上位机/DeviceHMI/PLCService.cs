using S7.Net;
using System.Timers;
using System.IO;
using System.Reflection;
using System.Linq;

namespace DeviceHMI;

public static class FileLog
{
    private static readonly string _path = Path.Combine(Path.GetTempPath(), "hmi_debug.log");
    public static void Write(string msg)
    {
        var line = $"[{DateTime.Now:HH:mm:ss.fff}] {msg}";
        File.AppendAllText(_path, line + Environment.NewLine);
        Console.WriteLine(line);
    }
}

public class DeviceData
{
    public int Id { get; set; }
    public string Name { get; set; } = "";
    public double Temperature { get; set; }
    public double Vibration { get; set; }
    public double Pressure { get; set; }
    public int Status { get; set; }
    public double CumulativeEnergy { get; set; }
}

public class PLCService
{
    // 从 mode.txt 读取模式：sim=仿真  plc=真实PLC
    // 文件不存在时自动创建，默认仿真模式。改文件即时生效，无需重编译。
    private static readonly bool SimulationMode = ReadSimulationMode();

    private static bool ReadSimulationMode()
    {
        string path = Path.Combine(Directory.GetCurrentDirectory(), "mode.txt");
        try
        {
            if (File.Exists(path))
            {
                string content = File.ReadAllText(path).Trim().ToLower();
                bool sim = !content.StartsWith("plc");
                FileLog.Write($"[Init] mode.txt: {(sim ? "仿真" : "真实PLC")}");
                return sim;
            }
        }
        catch { /* permission error, use default */ }
        File.WriteAllText(path, "sim");
        FileLog.Write("[Init] 已创建 mode.txt，默认仿真模式。改第一行为 plc 可切换真实PLC模式。");
        return true;
    }

    private Plc? _plc;
    private System.Net.Sockets.TcpClient? _tcp;
    private System.Net.Sockets.NetworkStream? _netStream;
    private readonly Random _rng = new();
    private readonly DeviceData[] _devices;
    private double[] _baseTemp;
    private double[] _baseVib;

    public bool Connected => SimulationMode || (_plc?.IsConnected ?? false);
    public string ConnectionStatus => SimulationMode ? "仿真模式" :
        (_plc?.IsConnected ?? false) ? "已连接 PLC S7-1200" : "未连接";

    public DeviceData[] Devices => _devices;

    private static readonly string[] DeviceNames =
        ["数控铣床", "数控车床", "磨床", "钻床", "冲压机", "注塑机"];

    public PLCService()
    {
        _devices = new DeviceData[6];
        _baseTemp = new double[6];
        _baseVib = new double[6];
        for (int i = 0; i < 6; i++)
        {
            _devices[i] = new DeviceData { Id = i + 1, Name = DeviceNames[i] };
            _baseTemp[i] = 35 + _rng.NextDouble() * 20;
            _baseVib[i] = 1.0 + _rng.NextDouble() * 2.0;
        }

        if (!SimulationMode)
        {
            _plc = new Plc(CpuType.S71200, "192.168.1.13", 0, 1);
        }
    }

    // 可靠读取4字节 — NetworkStream.Read可能返回<4
    private static byte[] ReadExactly(System.Net.Sockets.NetworkStream s, int count)
    {
        var buf = new byte[count];
        int total = 0;
        while (total < count)
        {
            int n = s.Read(buf, total, count - total);
            if (n == 0) throw new IOException("Connection closed");
            total += n;
        }
        return buf;
    }

    public bool Connect()
    {
        if (SimulationMode) return true;
        try
        {
            _tcp = new System.Net.Sockets.TcpClient();
            _tcp.Connect("192.168.1.13", 102);
            _netStream = _tcp.GetStream();

            // 手工 COTP 握手 (S7-1200 TSAP: 0x0100 / 0x0100)
            byte[] cotpCr = [
                0x03, 0x00, 0x00, 0x16,     // TPKT: ver=3, len=22
                0x11, 0xE0,                 // COTP: len=17, CR
                0x00, 0x00,                 // dst ref
                0x00, 0x01,                 // src ref
                0x00,                       // class 0
                0xC0, 0x01, 0x0A,           // TPDU size: 960
                0xC1, 0x02, 0x01, 0x00,     // src TSAP
                0xC2, 0x02, 0x01, 0x00      // dst TSAP
            ];
            _netStream.Write(cotpCr, 0, cotpCr.Length);

            // 读 TPKT header → COTP CC
            var tpkt = ReadExactly(_netStream, 4);
            if (tpkt[0] != 3) throw new IOException($"Bad TPKT ver={tpkt[0]}");
            int cotpLen = (tpkt[2] << 8) | tpkt[3];
            var cotpBody = ReadExactly(_netStream, cotpLen - 4);
            FileLog.Write($"[PLC] COTP CC: tpkt={tpkt[0]:X2}{tpkt[1]:X2}{tpkt[2]:X2}{tpkt[3]:X2} cotp={cotpBody[0]:X2}{cotpBody[1]:X2} len={cotpLen}");

            // S7 Setup Communication
            byte[] s7Setup = [
                0x03, 0x00, 0x00, 0x19,     // TPKT: ver=3, len=25
                0x02, 0xF0, 0x80,           // S7 header
                0x32, 0x01, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x08, 0x00, 0x00, 0xF0, 0x00,
                0x00, 0x01, 0x00, 0x01, 0x01, 0xE0    // PDU size 480
            ];
            _netStream.Write(s7Setup, 0, s7Setup.Length);
            var s7tpkt = ReadExactly(_netStream, 4);
            int s7Len = (s7tpkt[2] << 8) | s7tpkt[3];
            var s7Body = ReadExactly(_netStream, s7Len - 4);
            FileLog.Write($"[PLC] S7 Setup ok, response len={s7Len}");

            // 注入已连接的 socket 到 S7netplus
            typeof(Plc)
                .GetField("tcpClient", BindingFlags.NonPublic | BindingFlags.Instance)!
                .SetValue(_plc, _tcp);
            typeof(Plc)
                .GetField("_stream", BindingFlags.NonPublic | BindingFlags.Instance)!
                .SetValue(_plc, _netStream);

            // 设置 MaxPDUSize 以匹配协商值
            typeof(Plc)
                .GetField("<MaxPDUSize>k__BackingField", BindingFlags.NonPublic | BindingFlags.Instance)!
                .SetValue(_plc, 480);

            FileLog.Write($"[PLC] Connect OK, IsConnected={_plc?.IsConnected}");

            if (_plc?.IsConnected == true)
            {
                for (int db = 1; db <= 4; db++)
                {
                    try
                    {
                        var test = _plc.ReadBytes(DataType.DataBlock, db, 0, 4);
                        FileLog.Write($"[PLC] DB{db} read OK len={test.Length}");
                    }
                    catch (Exception ex)
                    {
                        FileLog.Write($"[PLC] DB{db} read failed: {ex.Message.Split('\n')[0].Trim()}");
                    }
                }
                try
                {
                    var test = _plc.ReadBytes(DataType.Memory, 0, 0, 10);
                    FileLog.Write($"[PLC] M0 read OK len={test.Length}");
                }
                catch (Exception ex)
                {
                    FileLog.Write($"[PLC] M0 read failed: {ex.Message.Split('\n')[0].Trim()}");
                }
            }
            return _plc?.IsConnected ?? false;
        }
        catch (Exception ex) { FileLog.Write($"[PLC] Connect failed: {ex.Message}"); return false; }
    }

    public void Disconnect()
    {
        if (!SimulationMode)
        {
            _plc?.Close();
            _netStream?.Close();
            _tcp?.Close();
        }
    }

    public bool ReadAll()
    {
        if (SimulationMode)
        {
            SimulateData();
            return true;
        }

        try
        {
            var bytes = _plc!.ReadBytes(DataType.DataBlock, 1, 0, 108);
            if (bytes == null || bytes.Length < 108)
            {
                FileLog.Write($"[PLC] ReadBytes failed: {(bytes == null ? "null" : $"len={bytes.Length}")}");
                return false;
            }

            FileLog.Write($"[PLC] ReadAll OK, first 8 bytes: {bytes[0]:X2} {bytes[1]:X2} {bytes[2]:X2} {bytes[3]:X2} {bytes[4]:X2} {bytes[5]:X2} {bytes[6]:X2} {bytes[7]:X2}");

            for (int i = 0; i < 6; i++)
            {
                int off = i * 18;
                var dev = _devices[i];
                dev.Temperature = S7.Net.Types.Real.FromByteArray(new[] { bytes[off], bytes[off + 1], bytes[off + 2], bytes[off + 3] });
                dev.Vibration = S7.Net.Types.Real.FromByteArray(new[] { bytes[off + 4], bytes[off + 5], bytes[off + 6], bytes[off + 7] });
                dev.Pressure = S7.Net.Types.Real.FromByteArray(new[] { bytes[off + 8], bytes[off + 9], bytes[off + 10], bytes[off + 11] });
                dev.Status = (bytes[off + 12] << 8) | bytes[off + 13];
                dev.CumulativeEnergy = S7.Net.Types.Real.FromByteArray(new[] { bytes[off + 14], bytes[off + 15], bytes[off + 16], bytes[off + 17] });
            }
            ApplyDynamicVariation();
            return true;
        }
        catch (Exception ex)
        {
            FileLog.Write($"[PLC] ReadAll exception: {ex.Message.Split('\n')[0].Trim()}");
            return false;
        }
    }

    private void SimulateData()
    {
        for (int i = 0; i < 6; i++)
        {
            var dev = _devices[i];
            _baseTemp[i] += (_rng.NextDouble() - 0.5) * 8;
            _baseTemp[i] = Math.Clamp(_baseTemp[i], 25, 95);
            double temp = _baseTemp[i] + (_rng.NextDouble() > 0.85 ? _rng.NextDouble() * 15 : 0);
            dev.Temperature = Math.Round(temp, 1);

            _baseVib[i] += (_rng.NextDouble() - 0.5) * 0.8;
            _baseVib[i] = Math.Clamp(_baseVib[i], 0.3, 9.0);
            dev.Vibration = Math.Round(_baseVib[i], 1);

            dev.Pressure = Math.Round(0.2 + _rng.NextDouble() * 0.7, 2);

            if (dev.Temperature > 80) dev.Status = 2;
            else if (dev.Temperature > 60) dev.Status = 1;
            else dev.Status = 0;

            double power = dev.Status switch { 2 => 0.5, 1 => 2.5, _ => 3.0 + _rng.NextDouble() };
            dev.CumulativeEnergy += Math.Round(power * 2.0 / 3600.0, 4);
        }
    }

    private bool _baselineInit;

    private void ApplyDynamicVariation()
    {
        if (!_baselineInit)
        {
            for (int i = 0; i < 6; i++)
            {
                _baseTemp[i] = _devices[i].Temperature;
                _baseVib[i] = _devices[i].Vibration;
            }
            _baselineInit = true;
        }

        for (int i = 0; i < 6; i++)
        {
            var dev = _devices[i];
            _baseTemp[i] += (_rng.NextDouble() - 0.5) * 8;
            _baseTemp[i] = Math.Clamp(_baseTemp[i], 25, 95);
            dev.Temperature = Math.Round(_baseTemp[i] + (_rng.NextDouble() > 0.9 ? _rng.NextDouble() * 15 : 0), 1);

            _baseVib[i] += (_rng.NextDouble() - 0.5) * 0.8;
            _baseVib[i] = Math.Clamp(_baseVib[i], 0.3, 9.0);
            dev.Vibration = Math.Round(_baseVib[i], 1);

            dev.Pressure = Math.Round(dev.Pressure + (_rng.NextDouble() - 0.5) * 0.1, 2);

            if (dev.Temperature > 80) dev.Status = 2;
            else if (dev.Temperature > 60) dev.Status = 1;
            else dev.Status = 0;

            double power = dev.Status switch { 2 => 0.5, 1 => 2.5, _ => 3.0 + _rng.NextDouble() };
            dev.CumulativeEnergy += Math.Round(power * 2.0 / 3600.0, 4);
        }
    }

    public void SetDeviceRunning(int deviceId, bool run)
    {
        if (!SimulationMode && _plc?.IsConnected == true)
        {
            int byteAddr = 108 + deviceId - 1;
            byte val = run ? (byte)1 : (byte)0;
            _plc.Write(DataType.Memory, 0, byteAddr, val);
        }
        if (SimulationMode && !run)
        {
            _baseTemp[deviceId - 1] = 25;
        }
    }
}
