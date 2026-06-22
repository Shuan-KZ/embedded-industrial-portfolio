package com.example.demo.controller;

import com.example.demo.entity.AlarmRecord;
import com.example.demo.entity.Result;
import com.example.demo.service.AlarmRecordService;
import org.springframework.web.bind.annotation.*;
import java.util.List;

@RestController
@RequestMapping("/api")
public class AlarmController {

    private final AlarmRecordService service;

    public AlarmController(AlarmRecordService service) {
        this.service = service;
    }

    @GetMapping("/alarms")
    public Result<List<AlarmRecord>> list() {
        return Result.ok(service.findAll());
    }

    @GetMapping("/alarms/device/{id}")
    public Result<List<AlarmRecord>> byDevice(@PathVariable Long id) {
        return Result.ok(service.findByDeviceId(id));
    }

    @PutMapping("/alarms/{id}/resolve")
    public Result<String> resolve(@PathVariable Long id) {
        service.resolve(id);
        return Result.ok("OK");
    }
}
