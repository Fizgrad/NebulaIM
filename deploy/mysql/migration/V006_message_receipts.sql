CREATE TABLE IF NOT EXISTS message_receipts (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    message_id BIGINT UNSIGNED NOT NULL,
    user_id BIGINT UNSIGNED NOT NULL,
    delivered_at BIGINT NOT NULL DEFAULT 0,
    read_at BIGINT NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    UNIQUE KEY uk_message_user (message_id, user_id),
    INDEX idx_user_read (user_id, read_at),
    INDEX idx_message_read (message_id, read_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
