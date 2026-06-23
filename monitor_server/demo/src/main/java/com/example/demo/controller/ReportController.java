package com.example.demo.controller;

import com.example.demo.entity.AlarmRecord;
import com.example.demo.entity.DeviceData;
import com.example.demo.entity.DeviceInfo;
import com.example.demo.entity.Result;
import com.example.demo.service.AlarmRecordService;
import com.example.demo.service.DeviceDataService;
import com.example.demo.service.DeviceInfoService;
import jakarta.servlet.http.HttpServletResponse;
import org.apache.poi.ss.usermodel.*;
import org.apache.poi.ss.util.CellRangeAddress;
import org.apache.poi.xssf.usermodel.XSSFWorkbook;
import org.springframework.web.bind.annotation.*;

import java.io.OutputStream;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.time.LocalDate;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.*;

@RestController
@RequestMapping("/api")
public class ReportController {

    private final DeviceInfoService deviceInfoService;
    private final DeviceDataService dataService;
    private final AlarmRecordService alarmService;

    public ReportController(DeviceInfoService deviceInfoService, DeviceDataService dataService, AlarmRecordService alarmService) {
        this.deviceInfoService = deviceInfoService;
        this.dataService = dataService;
        this.alarmService = alarmService;
    }

    // ==================== OEE ====================

    @GetMapping("/oee")
    public Result<Map<String, Object>> getOEE(@RequestParam(required = false, defaultValue = "24") int periodHours) {
        List<DeviceInfo> devices = deviceInfoService.findAll();
        List<Map<String, Object>> oeeList = new ArrayList<>();
        double avgOEE = 0;
        int count = 0;

        for (DeviceInfo dev : devices) {
            Map<String, Object> item = new LinkedHashMap<>();
            item.put("deviceId", dev.getId());
            item.put("deviceName", dev.getDeviceName());

            DeviceData latest = dataService.findLatest(dev.getId());
            List<DeviceData> recent = dataService.findRange(dev.getId(),
                    LocalDateTime.now().minusHours(periodHours).format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")),
                    LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")));

            double availability = 100.0;
            if (recent != null && !recent.isEmpty()) {
                long faultCount = recent.stream().filter(d -> d.getStatus() != null && d.getStatus() == 2).count();
                availability = Math.round(10000.0 * (recent.size() - faultCount) / recent.size()) / 100.0;
            }
            if (latest != null && latest.getStatus() != null && latest.getStatus() == 2) {
                availability = Math.min(availability, 90.0);
            }

            double performance = 85.0;
            if (recent != null && recent.size() >= 2) {
                DeviceData first = recent.get(0);
                DeviceData last = recent.get(recent.size() - 1);
                if (first.getCumulativeEnergy() != null && last.getCumulativeEnergy() != null
                        && first.getCollectTime() != null && last.getCollectTime() != null) {
                    double energyDelta = last.getCumulativeEnergy() - first.getCumulativeEnergy();
                    double hours = java.time.Duration.between(
                            first.getCollectTime(), last.getCollectTime()).toSeconds() / 3600.0;
                    if (hours > 0.01 && energyDelta > 0) {
                        double expectedEnergy = 3.5 * hours * (availability / 100.0);
                        if (expectedEnergy > 0.001) {
                            performance = Math.min(100.0, Math.max(50.0,
                                    Math.round(energyDelta / expectedEnergy * 1000.0) / 10.0));
                        }
                    }
                }
            }
            if (latest != null && latest.getStatus() != null && latest.getStatus() == 2) {
                performance = Math.min(performance, 55.0);
            }

            double quality = 95.0;
            if (latest != null) {
                double vib = latest.getVibration() != null ? latest.getVibration() : 1.0;
                double temp = latest.getTemperature() != null ? latest.getTemperature() : 40.0;
                double vibPenalty = Math.min(25, Math.max(0, (vib - 1.0) * 4.5));
                double tempPenalty = Math.max(0, (temp - 55.0) * 0.6);
                quality = Math.round(Math.max(60.0, 100.0 - vibPenalty - tempPenalty) * 10) / 10.0;
            }

            double oee = availability * performance * quality / 10000.0;
            item.put("availability", Math.round(availability * 10) / 10.0);
            item.put("performance", Math.round(performance * 10) / 10.0);
            item.put("quality", Math.round(quality * 10) / 10.0);
            item.put("oee", Math.round(oee * 10) / 10.0);
            item.put("status", latest != null ? latest.getStatus() : 0);

            oeeList.add(item);
            avgOEE += oee;
            count++;
        }

        Map<String, Object> result = new LinkedHashMap<>();
        result.put("devices", oeeList);
        result.put("avgOEE", count > 0 ? Math.round(avgOEE / count * 10) / 10.0 : 0);
        result.put("time", LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")));
        return Result.ok(result);
    }

    // ==================== Excel ====================

    @GetMapping("/export/daily-report")
    public void exportDailyReport(@RequestParam(required = false) String date, HttpServletResponse response) {
        try {
            LocalDate reportDate = (date != null) ? LocalDate.parse(date) : LocalDate.now();
            String startStr = reportDate.atStartOfDay().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss"));
            String endStr = reportDate.atTime(23, 59, 59).format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss"));
            String dateLabel = reportDate.format(DateTimeFormatter.ofPattern("yyyy-MM-dd"));

            List<DeviceInfo> devices = deviceInfoService.findAll();
            Workbook wb = new XSSFWorkbook();

            CellStyle titleStyle = wb.createCellStyle();
            Font titleFont = wb.createFont();
            titleFont.setBold(true); titleFont.setFontHeightInPoints((short) 16);
            titleStyle.setFont(titleFont);
            titleStyle.setAlignment(HorizontalAlignment.CENTER);

            CellStyle headerStyle = wb.createCellStyle();
            Font headerFont = wb.createFont();
            headerFont.setBold(true); headerFont.setFontHeightInPoints((short) 11);
            headerStyle.setFont(headerFont);
            headerStyle.setFillForegroundColor(IndexedColors.GREY_25_PERCENT.getIndex());
            headerStyle.setFillPattern(FillPatternType.SOLID_FOREGROUND);
            headerStyle.setBorderBottom(BorderStyle.THIN);
            headerStyle.setBorderTop(BorderStyle.THIN);
            headerStyle.setBorderLeft(BorderStyle.THIN);
            headerStyle.setBorderRight(BorderStyle.THIN);

            CellStyle dataStyle = wb.createCellStyle();
            dataStyle.setBorderBottom(BorderStyle.THIN);
            dataStyle.setBorderTop(BorderStyle.THIN);
            dataStyle.setBorderLeft(BorderStyle.THIN);
            dataStyle.setBorderRight(BorderStyle.THIN);

            // Sheet 1
            Sheet sheet1 = wb.createSheet("Device Run Data");
            Row titleRow1 = sheet1.createRow(0);
            titleRow1.createCell(0).setCellValue("Device Run Data Daily Report — " + dateLabel);
            titleRow1.getCell(0).setCellStyle(titleStyle);
            sheet1.addMergedRegion(new CellRangeAddress(0, 0, 0, 6));

            Row header1 = sheet1.createRow(2);
            String[] cols1 = {"Device Name", "Device Code", "Temp(C)", "Vibration(mm/s)", "Pressure(MPa)", "Cumul.Energy(kWh)", "Status"};
            for (int i = 0; i < cols1.length; i++) {
                Cell c = header1.createCell(i); c.setCellValue(cols1[i]); c.setCellStyle(headerStyle);
            }

            int rowIdx = 3;
            for (DeviceInfo dev : devices) {
                DeviceData latest = dataService.findLatest(dev.getId());
                Row r = sheet1.createRow(rowIdx++);
                r.createCell(0).setCellValue(dev.getDeviceName());
                r.getCell(0).setCellStyle(dataStyle);
                r.createCell(1).setCellValue(dev.getDeviceCode());
                r.getCell(1).setCellStyle(dataStyle);
                Cell c2 = r.createCell(2);
                c2.setCellValue(latest != null && latest.getTemperature() != null ? latest.getTemperature() : 0);
                c2.setCellStyle(dataStyle);
                Cell c3 = r.createCell(3);
                c3.setCellValue(latest != null && latest.getVibration() != null ? latest.getVibration() : 0);
                c3.setCellStyle(dataStyle);
                Cell c4 = r.createCell(4);
                c4.setCellValue(latest != null && latest.getPressure() != null ? latest.getPressure() : 0);
                c4.setCellStyle(dataStyle);
                Cell c5 = r.createCell(5);
                c5.setCellValue(latest != null && latest.getCumulativeEnergy() != null ? latest.getCumulativeEnergy() : 0);
                c5.setCellStyle(dataStyle);
                Cell c6 = r.createCell(6);
                int status = latest != null && latest.getStatus() != null ? latest.getStatus() : 0;
                c6.setCellValue(status == 0 ? "Normal" : (status == 1 ? "Warning" : "Fault"));
                c6.setCellStyle(dataStyle);
            }

            for (int i = 0; i < cols1.length; i++) sheet1.setColumnWidth(i, 16 * 256);
            sheet1.setColumnWidth(0, 22 * 256);

            // Sheet 2
            Sheet sheet2 = wb.createSheet("Alarm Records");
            Row titleRow2 = sheet2.createRow(0);
            titleRow2.createCell(0).setCellValue("Alarm Records Summary — " + dateLabel);
            titleRow2.getCell(0).setCellStyle(titleStyle);
            sheet2.addMergedRegion(new CellRangeAddress(0, 0, 0, 5));

            Row header2 = sheet2.createRow(2);
            String[] cols2 = {"Device Name", "Alarm Type", "Alarm Value", "Threshold", "Description", "Trigger Time"};
            for (int i = 0; i < cols2.length; i++) {
                Cell c = header2.createCell(i); c.setCellValue(cols2[i]); c.setCellStyle(headerStyle);
            }

            Map<Long, String> devNameMap = new HashMap<>();
            for (DeviceInfo d : devices) devNameMap.put(d.getId(), d.getDeviceName());

            List<AlarmRecord> alarms = alarmService.findAll();
            int rowIdx2 = 3;
            for (AlarmRecord a : alarms) {
                Row r = sheet2.createRow(rowIdx2++);
                r.createCell(0).setCellValue(devNameMap.getOrDefault(a.getDeviceId(), "Unknown"));
                r.getCell(0).setCellStyle(dataStyle);
                r.createCell(1).setCellValue(a.getAlarmType() != null ? a.getAlarmType() : "");
                r.getCell(1).setCellStyle(dataStyle);
                Cell ca2 = r.createCell(2);
                ca2.setCellValue(a.getAlarmValue() != null ? a.getAlarmValue() : 0);
                ca2.setCellStyle(dataStyle);
                Cell ca3 = r.createCell(3);
                ca3.setCellValue(a.getThresholdValue() != null ? a.getThresholdValue() : 0);
                ca3.setCellStyle(dataStyle);
                r.createCell(4).setCellValue(a.getMessage() != null ? a.getMessage() : "");
                r.getCell(4).setCellStyle(dataStyle);
                r.createCell(5).setCellValue(a.getTriggerTime() != null ? a.getTriggerTime().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")) : "");
                r.getCell(5).setCellStyle(dataStyle);
            }

            for (int i = 0; i < cols2.length; i++) sheet2.setColumnWidth(i, 16 * 256);
            sheet2.setColumnWidth(4, 35 * 256);

            // Sheet 3
            Map<String, Object> oeeData = getOEE(24).getData();
            @SuppressWarnings("unchecked")
            List<Map<String, Object>> oeeList = (List<Map<String, Object>>) oeeData.get("devices");

            Sheet sheet3 = wb.createSheet("OEE Efficiency");
            Row titleRow3 = sheet3.createRow(0);
            titleRow3.createCell(0).setCellValue("OEE Overall Equipment Effectiveness — " + dateLabel);
            titleRow3.getCell(0).setCellStyle(titleStyle);
            sheet3.addMergedRegion(new CellRangeAddress(0, 0, 0, 4));

            Row header3 = sheet3.createRow(2);
            String[] cols3 = {"Device Name", "Availability(%)", "Performance(%)", "Quality(%)", "OEE(%)"};
            for (int i = 0; i < cols3.length; i++) {
                Cell c = header3.createCell(i); c.setCellValue(cols3[i]); c.setCellStyle(headerStyle);
            }

            int rowIdx3 = 3;
            for (Map<String, Object> o : oeeList) {
                Row r = sheet3.createRow(rowIdx3++);
                r.createCell(0).setCellValue((String) o.get("deviceName"));
                r.getCell(0).setCellStyle(dataStyle);
                Cell c1 = r.createCell(1);
                c1.setCellValue(((Number) o.get("availability")).doubleValue());
                c1.setCellStyle(dataStyle);
                Cell c2 = r.createCell(2);
                c2.setCellValue(((Number) o.get("performance")).doubleValue());
                c2.setCellStyle(dataStyle);
                Cell c3 = r.createCell(3);
                c3.setCellValue(((Number) o.get("quality")).doubleValue());
                c3.setCellStyle(dataStyle);
                Cell c4 = r.createCell(4);
                c4.setCellValue(((Number) o.get("oee")).doubleValue());
                c4.setCellStyle(dataStyle);
            }

            Row avgRow = sheet3.createRow(rowIdx3);
            Cell avgCell = avgRow.createCell(0);
            avgCell.setCellValue("Average OEE");
            avgCell.setCellStyle(headerStyle);
            Cell avgVal = avgRow.createCell(4);
            avgVal.setCellValue(((Number) oeeData.get("avgOEE")).doubleValue());
            avgVal.setCellStyle(headerStyle);

            for (int i = 0; i < cols3.length; i++) sheet3.setColumnWidth(i, 16 * 256);
            sheet3.setColumnWidth(0, 22 * 256);

            response.setContentType("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
            String filename = "Daily_Report_" + dateLabel + ".xlsx";
            response.setHeader("Content-Disposition", "attachment; filename*=UTF-8''" + URLEncoder.encode(filename, StandardCharsets.UTF_8));
            OutputStream os = response.getOutputStream();
            wb.write(os);
            wb.close();
            os.flush();
        } catch (Exception e) {
            e.printStackTrace();
            try {
                response.reset();
                response.setStatus(HttpServletResponse.SC_INTERNAL_SERVER_ERROR);
                response.setContentType("application/json;charset=UTF-8");
                response.getWriter().write("{\"code\":500,\"msg\":\"Excel generation failed: " + e.getMessage() + "\"}");
            } catch (Exception ignored) {}
        }
    }
}
