package com.example.demo.controller;

import com.example.demo.entity.DeviceInfo;
import com.example.demo.entity.Result;
import com.example.demo.service.DeviceInfoService;
import jakarta.validation.Valid;
import org.springframework.web.bind.annotation.*;
import java.util.List;
import java.util.Map;

@RestController
public class DeviceInfoController {

    private final DeviceInfoService service;

    public DeviceInfoController(DeviceInfoService service) {
        this.service = service;
    }

    // ======= Plan API (/api/*) =======

    @GetMapping("/api/devices")
    public Result<Map<String, Object>> listWithData() {
        return Result.ok(service.getDevicesWithLatestData());
    }

    @GetMapping("/api/devices/{id}")
    public Result<Map<String, Object>> getById(@PathVariable Long id) {
        Map<String, Object> all = service.getDevicesWithLatestData();
        @SuppressWarnings("unchecked")
        List<Map<String, Object>> devices = (List<Map<String, Object>>) all.get("devices");
        for (Map<String, Object> d : devices) {
            if (d.get("id").equals(id)) {
                return Result.ok(d);
            }
        }
        return Result.fail(404, "设备不存在");
    }

    // ======= Old CRUD endpoints (compatible with management page) =======

    @GetMapping("/device/list")
    public Result<List<DeviceInfo>> list() {
        return Result.ok(service.findAll());
    }

    @GetMapping("/device/{id}")
    public Result<DeviceInfo> getRawById(@PathVariable Long id) {
        DeviceInfo d = service.findById(id);
        if (d == null) return Result.fail(404, "设备不存在");
        return Result.ok(d);
    }

    @PostMapping("/device/add")
    public Result<String> add(@Valid @RequestBody DeviceInfo device) {
        service.add(device);
        return Result.ok("OK");
    }

    @PutMapping("/device/update")
    public Result<String> update(@Valid @RequestBody DeviceInfo device) {
        service.update(device);
        return Result.ok("OK");
    }

    @DeleteMapping("/device/delete/{id}")
    public Result<String> delete(@PathVariable Long id) {
        service.delete(id);
        return Result.ok("OK");
    }
}
