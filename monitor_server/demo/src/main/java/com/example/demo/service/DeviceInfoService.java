package com.example.demo.service;

import com.example.demo.entity.DeviceData;
import com.example.demo.entity.DeviceInfo;
import com.example.demo.mapper.DeviceInfoMapper;
import org.springframework.stereotype.Service;
import java.util.*;

@Service
public class DeviceInfoService {

    private final DeviceInfoMapper mapper;
    private final DeviceDataService dataService;

    public DeviceInfoService(DeviceInfoMapper mapper, DeviceDataService dataService) {
        this.mapper = mapper;
        this.dataService = dataService;
    }

    public List<DeviceInfo> findAll() {
        return mapper.findAll();
    }

    public DeviceInfo findById(Long id) {
        return mapper.findById(id);
    }

    public int add(DeviceInfo device) {
        return mapper.insert(device);
    }

    public int update(DeviceInfo device) {
        return mapper.update(device);
    }

    public int delete(Long id) {
        return mapper.deleteById(id);
    }

    public Map<String, Object> getDevicesWithLatestData() {
        List<DeviceInfo> devices = mapper.findAll();
        List<Map<String, Object>> result = new ArrayList<>();
        int normal = 0, warning = 0, fault = 0;

        for (DeviceInfo d : devices) {
            DeviceData latest = dataService.findLatest(d.getId());
            Map<String, Object> item = new LinkedHashMap<>();
            item.put("id", d.getId());
            item.put("name", d.getDeviceName());
            item.put("code", d.getDeviceCode());
            item.put("type", d.getDeviceType());
            item.put("location", d.getLocation());
            item.put("ipAddress", d.getIpAddress());
            item.put("infoStatus", d.getStatus());

            if (latest != null) {
                item.put("temperature", latest.getTemperature());
                item.put("vibration", latest.getVibration());
                item.put("pressure", latest.getPressure());
                item.put("dataStatus", latest.getStatus());
                item.put("cumulativeEnergy", latest.getCumulativeEnergy());
                item.put("collectTime", latest.getCollectTime());
                int s = latest.getStatus() != null ? latest.getStatus() : 0;
                if (s == 0) normal++;
                else if (s == 1) warning++;
                else fault++;
            } else {
                item.put("temperature", null);
                item.put("vibration", null);
                item.put("pressure", null);
                item.put("dataStatus", 0);
                item.put("cumulativeEnergy", null);
                item.put("collectTime", null);
                normal++;
            }
            result.add(item);
        }

        Map<String, Object> response = new LinkedHashMap<>();
        response.put("totalDevices", devices.size());
        response.put("normalCount", normal);
        response.put("warningCount", warning);
        response.put("faultCount", fault);
        response.put("devices", result);
        return response;
    }
}
