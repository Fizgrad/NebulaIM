SET @has_old_history_index := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'messages'
      AND INDEX_NAME = 'idx_conversation_time'
);
SET @sql := IF(
    @has_old_history_index > 0,
    'ALTER TABLE messages DROP INDEX idx_conversation_time',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_history_cursor_index := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'messages'
      AND INDEX_NAME = 'idx_conversation_cursor'
);
SET @sql := IF(
    @has_history_cursor_index = 0,
    'ALTER TABLE messages ADD INDEX idx_conversation_cursor (conversation_id, created_at, message_id)',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
