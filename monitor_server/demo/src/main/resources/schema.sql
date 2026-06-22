-- ============================================
-- 工业设备监控系统 — H2 内嵌数据库初始化
-- 兼容模式: MySQL
-- ============================================

-- 1. 设备信息表
CREATE TABLE IF NOT EXISTS device_info (
    id BIGINT NOT NULL AUTO_INCREMENT,
    device_name VARCHAR(100) NOT NULL,
    device_code VARCHAR(50) NOT NULL,
    device_type VARCHAR(50) DEFAULT NULL,
    status VARCHAR(20) DEFAULT 'ONLINE',
    location VARCHAR(200) DEFAULT NULL,
    ip_address VARCHAR(50) DEFAULT NULL,
    create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    update_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id)
);

-- 2. 设备时序数据表
CREATE TABLE IF NOT EXISTS device_data (
    id BIGINT NOT NULL AUTO_INCREMENT,
    device_id BIGINT NOT NULL,
    temperature DOUBLE DEFAULT NULL,
    vibration DOUBLE DEFAULT NULL,
    pressure DOUBLE DEFAULT NULL,
    status INT DEFAULT 0,
    cumulative_energy DOUBLE DEFAULT NULL,
    collect_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id)
);
CREATE INDEX IF NOT EXISTS idx_device_time ON device_data (device_id, collect_time);

-- 3. 报警记录表
CREATE TABLE IF NOT EXISTS alarm_record (
    id BIGINT NOT NULL AUTO_INCREMENT,
    device_id BIGINT NOT NULL,
    alarm_type VARCHAR(50) DEFAULT NULL,
    alarm_value DOUBLE DEFAULT NULL,
    threshold_value DOUBLE DEFAULT NULL,
    message VARCHAR(500) DEFAULT NULL,
    resolved TINYINT DEFAULT 0,
    trigger_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    resolve_time DATETIME DEFAULT NULL,
    PRIMARY KEY (id)
);
CREATE INDEX IF NOT EXISTS idx_device_alarm ON alarm_record (device_id, trigger_time);

-- 初始设备（仅当表为空时插入）
INSERT INTO device_info (device_name, device_code, device_type, status, location, ip_address)
SELECT * FROM (
    SELECT '数控铣床' AS a, 'MC-001' AS b, 'CNC' AS c, 'ONLINE' AS d, 'A车间-1号工位' AS e, '192.168.1.101' AS f UNION ALL
    SELECT '数控车床', 'LT-001', 'CNC', 'ONLINE', 'A车间-2号工位', '192.168.1.102' UNION ALL
    SELECT '磨床', 'GR-001', 'GRINDER', 'ONLINE', 'B车间-1号工位', '192.168.2.101' UNION ALL
    SELECT '钻床', 'DR-001', 'DRILL', 'OFFLINE', 'B车间-2号工位', '192.168.2.102' UNION ALL
    SELECT '冲压机', 'PR-001', 'PRESS', 'ONLINE', 'C车间-1号工位', '192.168.3.101' UNION ALL
    SELECT '注塑机', 'IM-001', 'INJECTION', 'OFFLINE', 'C车间-2号工位', '192.168.3.102'
) AS t
WHERE NOT EXISTS (SELECT 1 FROM device_info);
