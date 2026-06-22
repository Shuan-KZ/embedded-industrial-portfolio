package com.example.demo.controller;

import com.example.demo.entity.AlarmRecord;
import com.example.demo.entity.DeviceData;
import com.example.demo.entity.Result;
import com.example.demo.service.AlarmRecordService;
import com.example.demo.service.DeviceDataService;
import org.springframework.web.bind.annotation.*;
import java.time.LocalDateTime;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

@RestController
@RequestMapping("/api")
public class DataController {

    private final DeviceDataService dataService;
    private final AlarmRecordService alarmService;
    private final Map<String, LocalDateTime> lastAlarmTime = new ConcurrentHashMap<>();

    public DataController(DeviceDataService dataService, AlarmRecordService alarmService) {
        this.dataService = dataService;
        this.alarmService = alarmService;
    }

    @PostMapping("/device-data")
    public Result<String> receiveData(@RequestBody Map<String, List<DeviceData>> body) {
        List<DeviceData> devices = body.get("devices");
        if (devices != null && !devices.isEmpty()) {
            for (DeviceData d : devices) {
                if (d.getCollectTime() == null) {
                    d.setCollectTime(LocalDateTime.now());
                }
            }
            dataService.saveBatch(devices);
            checkAlarms(devices);
        }
        return Result.ok("success");
    }

    @GetMapping("/device-data/range")
    public Result<List<DeviceData>> getRange(@RequestParam Long deviceId,
                                     @RequestParam(required = false) String start,
                                     @RequestParam(required = false) String end) {
        return Result.ok(dataService.findRange(deviceId, start, end));
    }

    private void checkAlarms(List<DeviceData> list) {
        LocalDateTime now = LocalDateTime.now();
        for (DeviceData d : list) {
            if (d.getTemperature() != null && d.getTemperature() > 80) {
                tryAddAlarm(d.getDeviceId(), "温度过高", d.getTemperature(), 80.0,
                        "设备" + d.getDeviceId() + " 温度过高: " + d.getTemperature() + "℃", now);
            } else if (d.getTemperature() != null && d.getTemperature() > 60) {
                tryAddAlarm(d.getDeviceId(), "温度偏高", d.getTemperature(), 60.0,
                        "设备" + d.getDeviceId() + " 温度偏高: " + d.getTemperature() + "℃", now);
            }
            if (d.getVibration() != null && d.getVibration() > 5.0) {
                tryAddAlarm(d.getDeviceId(), "振动异常", d.getVibration(), 5.0,
                        "设备" + d.getDeviceId() + " 振动异常: " + d.getVibration() + "mm/s", now);
            }
        }
    }

    private void tryAddAlarm(Long deviceId, String type, Double value, Double threshold,
                             String message, LocalDateTime now) {
        String key = deviceId + "-" + type;
        LocalDateTime last = lastAlarmTime.get(key);
        if (last != null && java.time.Duration.between(last, now).getSeconds() < 10) {
            return; // 10秒内同设备同类型不重复入库
        }
        lastAlarmTime.put(key, now);
        AlarmRecord alarm = new AlarmRecord();
        alarm.setDeviceId(deviceId);
        alarm.setAlarmType(type);
        alarm.setAlarmValue(value);
        alarm.setThresholdValue(threshold);
        alarm.setMessage(message);
        alarmService.add(alarm);
    }
}
