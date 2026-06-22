package com.example.demo.mapper;

import com.example.demo.entity.DeviceData;
import org.apache.ibatis.annotations.Mapper;
import java.util.List;

@Mapper
public interface DeviceDataMapper {
    int insert(DeviceData data);
    int insertBatch(List<DeviceData> list);
    List<DeviceData> findRange(Long deviceId, String start, String end);
    DeviceData findLatestByDeviceId(Long deviceId);
}
