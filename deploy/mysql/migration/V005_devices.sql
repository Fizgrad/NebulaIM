CREATE TABLE IF NOT EXISTS user_devices (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    device_id VARCHAR(128) NOT NULL,
    platform VARCHAR(32) NOT NULL,
    device_name VARCHAR(128) NOT NULL DEFAULT '',
    token_hash VARCHAR(64) NOT NULL DEFAULT '',
    last_login_at BIGINT NOT NULL,
    last_active_at BIGINT NOT NULL,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    UNIQUE KEY uk_user_device (user_id, device_id),
    INDEX idx_user_active (user_id, last_active_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
