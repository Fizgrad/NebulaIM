SET @has_recalled := (
    SELECT COUNT(*)
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'messages'
      AND COLUMN_NAME = 'recalled'
);
SET @ddl := IF(
    @has_recalled = 0,
    'ALTER TABLE messages ADD COLUMN recalled TINYINT NOT NULL DEFAULT 0 AFTER status',
    'SELECT 1'
);
PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_recalled_at := (
    SELECT COUNT(*)
    FROM information_schema.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'messages'
      AND COLUMN_NAME = 'recalled_at'
);
SET @ddl := IF(
    @has_recalled_at = 0,
    'ALTER TABLE messages ADD COLUMN recalled_at BIGINT NOT NULL DEFAULT 0 AFTER recalled',
    'SELECT 1'
);
PREPARE stmt FROM @ddl;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
