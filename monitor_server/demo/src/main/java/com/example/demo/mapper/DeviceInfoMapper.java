package com.example.demo.mapper;

import com.example.demo.entity.DeviceInfo;
import org.apache.ibatis.annotations.Mapper;
import java.util.List;

@Mapper
public interface DeviceInfoMapper {
    List<DeviceInfo> findAll();
    DeviceInfo findById(Long id);
    int insert(DeviceInfo device);
    int update(DeviceInfo device);
    int deleteById(Long id);
}
