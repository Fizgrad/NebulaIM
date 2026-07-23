SET @has_token := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'user_devices'
      AND COLUMN_NAME = 'token'
);

SET @has_token_hash := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'user_devices'
      AND COLUMN_NAME = 'token_hash'
);

SET @sql := IF(
    @has_token = 1 AND @has_token_hash = 0,
    'ALTER TABLE user_devices RENAME COLUMN token TO token_hash',
    'DO 0'
);

PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @sql := IF(
    @has_token = 0 AND @has_token_hash = 0,
    'ALTER TABLE user_devices ADD COLUMN token_hash VARCHAR(64) NOT NULL DEFAULT '''' AFTER device_name',
    'DO 0'
);

PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
