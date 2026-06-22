package com.example.demo.service;

import com.example.demo.entity.DeviceData;
import com.example.demo.mapper.DeviceDataMapper;
import org.springframework.stereotype.Service;
import java.util.List;

@Service
public class DeviceDataService {

    private final DeviceDataMapper mapper;

    public DeviceDataService(DeviceDataMapper mapper) {
        this.mapper = mapper;
    }

    public int save(DeviceData data) {
        return mapper.insert(data);
    }

    public int saveBatch(List<DeviceData> list) {
        if (list == null || list.isEmpty()) return 0;
        return mapper.insertBatch(list);
    }

    public List<DeviceData> findRange(Long deviceId, String start, String end) {
        return mapper.findRange(deviceId, start, end);
    }

    public DeviceData findLatest(Long deviceId) {
        return mapper.findLatestByDeviceId(deviceId);
    }
}
