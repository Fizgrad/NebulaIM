SET @has_client_sequence := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'messages'
      AND COLUMN_NAME = 'client_sequence_id'
);
SET @sql := IF(
    @has_client_sequence = 0,
    'ALTER TABLE messages ADD COLUMN client_sequence_id INT UNSIGNED NULL AFTER from_user_id',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_message_sequence_index := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'messages'
      AND INDEX_NAME = 'uk_message_sender_sequence'
);
SET @sql := IF(
    @has_message_sequence_index = 0,
    'ALTER TABLE messages ADD UNIQUE KEY uk_message_sender_sequence (from_user_id, client_sequence_id)',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_claim_token := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'outbox_events'
      AND COLUMN_NAME = 'claim_token'
);
SET @sql := IF(
    @has_claim_token = 0,
    'ALTER TABLE outbox_events ADD COLUMN claim_token VARCHAR(64) NOT NULL DEFAULT '''' AFTER trace_id',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_from_to_status_index := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'friend_requests'
      AND INDEX_NAME = 'uk_from_to_status'
);
SET @sql := IF(
    @has_from_to_status_index > 0,
    'ALTER TABLE friend_requests DROP INDEX uk_from_to_status',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_request_id := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'friend_requests'
      AND COLUMN_NAME = 'request_id'
);
SET @sql := IF(
    @has_request_id > 0,
    'ALTER TABLE friend_requests DROP COLUMN request_id',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_pending_low := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'friend_requests'
      AND COLUMN_NAME = 'pending_user_low'
);
SET @sql := IF(
    @has_pending_low = 0,
    'ALTER TABLE friend_requests ADD COLUMN pending_user_low BIGINT UNSIGNED GENERATED ALWAYS AS (CASE WHEN status = 0 THEN LEAST(from_user_id, to_user_id) ELSE NULL END) STORED',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_pending_high := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'friend_requests'
      AND COLUMN_NAME = 'pending_user_high'
);
SET @sql := IF(
    @has_pending_high = 0,
    'ALTER TABLE friend_requests ADD COLUMN pending_user_high BIGINT UNSIGNED GENERATED ALWAYS AS (CASE WHEN status = 0 THEN GREATEST(from_user_id, to_user_id) ELSE NULL END) STORED',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

SET @has_pending_index := (
    SELECT COUNT(*)
    FROM INFORMATION_SCHEMA.STATISTICS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'friend_requests'
      AND INDEX_NAME = 'uk_pending_friend_request'
);
SET @sql := IF(
    @has_pending_index = 0,
    'ALTER TABLE friend_requests ADD UNIQUE KEY uk_pending_friend_request (pending_user_low, pending_user_high)',
    'SELECT 1'
);
PREPARE stmt FROM @sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;
