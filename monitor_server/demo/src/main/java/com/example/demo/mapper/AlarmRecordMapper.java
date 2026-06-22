package com.example.demo.mapper;

import com.example.demo.entity.AlarmRecord;
import org.apache.ibatis.annotations.Mapper;
import java.util.List;

@Mapper
public interface AlarmRecordMapper {
    List<AlarmRecord> findAll();
    List<AlarmRecord> findByDeviceId(Long deviceId);
    int insert(AlarmRecord alarm);
    int resolve(Long id);
}
