CREATE TABLE IF NOT EXISTS friend_requests (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    from_user_id BIGINT UNSIGNED NOT NULL,
    to_user_id BIGINT UNSIGNED NOT NULL,
    message VARCHAR(255) NOT NULL DEFAULT '',
    status TINYINT NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    pending_user_low BIGINT UNSIGNED GENERATED ALWAYS AS (CASE WHEN status = 0 THEN LEAST(from_user_id, to_user_id) ELSE NULL END) STORED,
    pending_user_high BIGINT UNSIGNED GENERATED ALWAYS AS (CASE WHEN status = 0 THEN GREATEST(from_user_id, to_user_id) ELSE NULL END) STORED,
    UNIQUE KEY uk_pending_friend_request (pending_user_low, pending_user_high),
    INDEX idx_to_user_status_time (to_user_id, status, created_at),
    INDEX idx_from_user_time (from_user_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
