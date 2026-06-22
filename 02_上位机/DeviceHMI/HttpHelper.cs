using System.Text;
using System.Text.Json;

namespace DeviceHMI;

public class HttpHelper
{
    private static readonly HttpClient _client = new() { Timeout = TimeSpan.FromSeconds(5) };
    private const string DataUrl = "http://localhost:8081/api/device-data";

    public static async Task SendDeviceDataAsync(DeviceData[] devices)
    {
        try
        {
            var payload = new
            {
                devices = devices.Select(d => new
                {
                    deviceId = d.Id,
                    temperature = d.Temperature,
                    vibration = d.Vibration,
                    pressure = d.Pressure,
                    status = d.Status,
                    cumulativeEnergy = Math.Round(d.CumulativeEnergy, 2),
                    collectTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")
                })
            };

            var json = JsonSerializer.Serialize(payload);
            var content = new StringContent(json, Encoding.UTF8, "application/json");
            var resp = await _client.PostAsync(DataUrl, content);
            resp.EnsureSuccessStatusCode();
            FileLog.Write($"[HTTP] POST OK T1={devices[0].Temperature:F1} T2={devices[1].Temperature:F1} T3={devices[2].Temperature:F1}");
        }
        catch (Exception ex)
        {
            FileLog.Write($"[HTTP] POST failed: {ex.Message.Split('\n')[0].Trim()}");
        }
    }
}
