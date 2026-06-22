package com.example.demo.service;

import com.example.demo.entity.AlarmRecord;
import com.example.demo.mapper.AlarmRecordMapper;
import org.springframework.stereotype.Service;
import java.util.List;

@Service
public class AlarmRecordService {

    private final AlarmRecordMapper mapper;

    public AlarmRecordService(AlarmRecordMapper mapper) {
        this.mapper = mapper;
    }

    public List<AlarmRecord> findAll() {
        return mapper.findAll();
    }

    public List<AlarmRecord> findByDeviceId(Long deviceId) {
        return mapper.findByDeviceId(deviceId);
    }

    public int add(AlarmRecord alarm) {
        return mapper.insert(alarm);
    }

    public int resolve(Long id) {
        return mapper.resolve(id);
    }
}
