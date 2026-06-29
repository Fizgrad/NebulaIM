CREATE TABLE IF NOT EXISTS friend_requests (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    request_id BIGINT UNSIGNED NOT NULL UNIQUE,
    from_user_id BIGINT UNSIGNED NOT NULL,
    to_user_id BIGINT UNSIGNED NOT NULL,
    message VARCHAR(255) NOT NULL DEFAULT '',
    status TINYINT NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    UNIQUE KEY uk_from_to_status (from_user_id, to_user_id, status),
    INDEX idx_to_user_status_time (to_user_id, status, created_at),
    INDEX idx_from_user_time (from_user_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
