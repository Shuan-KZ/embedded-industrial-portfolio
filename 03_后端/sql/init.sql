-- ============================================
-- 工业设备监控系统 — 数据库初始化脚本
-- 数据库: device_monitor
-- 字符集: utf8mb4
-- ============================================

CREATE DATABASE IF NOT EXISTS device_monitor
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE device_monitor;

-- --------------------------------------------
-- 1. 设备信息表
-- --------------------------------------------
DROP TABLE IF EXISTS `device_info`;
CREATE TABLE `device_info` (
    `id` bigint NOT NULL AUTO_INCREMENT,
    `device_name` varchar(100) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '设备名称',
    `device_code` varchar(50) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT '设备编码',
    `device_type` varchar(50) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '设备类型(CNC/GRINDER/DRILL/PRESS/INJECTION)',
    `status` varchar(20) COLLATE utf8mb4_unicode_ci DEFAULT 'ONLINE' COMMENT 'ONLINE/OFFLINE/MAINTENANCE',
    `location` varchar(200) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '位置(车间-工位)',
    `ip_address` varchar(50) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT 'IP地址',
    `create_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='设备信息表';

-- 初始6台设备
INSERT INTO `device_info` (`device_name`, `device_code`, `device_type`, `status`, `location`, `ip_address`) VALUES
    ('数控铣床', 'MC-001', 'CNC',     'ONLINE',  'A车间-1号工位', '192.168.1.101'),
    ('数控车床', 'LT-001', 'CNC',     'ONLINE',  'A车间-2号工位', '192.168.1.102'),
    ('磨床',     'GR-001', 'GRINDER', 'ONLINE',  'B车间-1号工位', '192.168.2.101'),
    ('钻床',     'DR-001', 'DRILL',   'OFFLINE', 'B车间-2号工位', '192.168.2.102'),
    ('冲压机',   'PR-001', 'PRESS',   'ONLINE',  'C车间-1号工位', '192.168.3.101'),
    ('注塑机',   'IM-001', 'INJECTION','OFFLINE', 'C车间-2号工位', '192.168.3.102');

-- --------------------------------------------
-- 2. 设备时序数据表
-- --------------------------------------------
DROP TABLE IF EXISTS `device_data`;
CREATE TABLE `device_data` (
    `id` bigint NOT NULL AUTO_INCREMENT,
    `device_id` bigint NOT NULL COMMENT '关联device_info.id',
    `temperature` double DEFAULT NULL COMMENT '温度(°C)',
    `vibration` double DEFAULT NULL COMMENT '振动(mm/s)',
    `pressure` double DEFAULT NULL COMMENT '压力(MPa)',
    `status` int DEFAULT 0 COMMENT '0正常 1预警 2故障',
    `cumulative_energy` double DEFAULT NULL COMMENT '累计能耗(kWh)',
    `collect_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '采集时间',
    PRIMARY KEY (`id`),
    KEY `idx_device_time` (`device_id`, `collect_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='设备时序数据';

-- --------------------------------------------
-- 3. 报警记录表
-- --------------------------------------------
DROP TABLE IF EXISTS `alarm_record`;
CREATE TABLE `alarm_record` (
    `id` bigint NOT NULL AUTO_INCREMENT,
    `device_id` bigint NOT NULL COMMENT '关联device_info.id',
    `alarm_type` varchar(50) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '报警类型(温度过高/温度偏高/振动异常)',
    `alarm_value` double DEFAULT NULL COMMENT '触发值',
    `threshold_value` double DEFAULT NULL COMMENT '阈值',
    `message` varchar(500) COLLATE utf8mb4_unicode_ci DEFAULT NULL COMMENT '报警描述',
    `resolved` tinyint DEFAULT 0 COMMENT '0未恢复 1已恢复',
    `trigger_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '触发时间',
    `resolve_time` datetime DEFAULT NULL COMMENT '恢复时间',
    PRIMARY KEY (`id`),
    KEY `idx_device_alarm` (`device_id`, `trigger_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='报警记录';
