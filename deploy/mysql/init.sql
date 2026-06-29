CREATE DATABASE IF NOT EXISTS nebula_im CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS 'nebula'@'%' IDENTIFIED BY 'nebula';
CREATE USER IF NOT EXISTS 'nebula'@'localhost' IDENTIFIED BY 'nebula';
GRANT ALL PRIVILEGES ON nebula_im.* TO 'nebula'@'%';
GRANT ALL PRIVILEGES ON nebula_im.* TO 'nebula'@'localhost';
FLUSH PRIVILEGES;

USE nebula_im;

CREATE TABLE IF NOT EXISTS users (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    nickname VARCHAR(64) NOT NULL DEFAULT '',
    avatar VARCHAR(255) NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS friendships (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    friend_id BIGINT UNSIGNED NOT NULL,
    status TINYINT NOT NULL DEFAULT 1,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    UNIQUE KEY uk_user_friend (user_id, friend_id),
    INDEX idx_friend_id (friend_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `groups` (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    group_name VARCHAR(128) NOT NULL,
    owner_id BIGINT UNSIGNED NOT NULL,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    INDEX idx_owner_id (owner_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS group_members (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    group_id BIGINT UNSIGNED NOT NULL,
    user_id BIGINT UNSIGNED NOT NULL,
    role TINYINT NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    UNIQUE KEY uk_group_user (group_id, user_id),
    INDEX idx_user_id (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS messages (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    message_id BIGINT UNSIGNED NOT NULL UNIQUE,
    conversation_id BIGINT UNSIGNED NOT NULL,
    from_user_id BIGINT UNSIGNED NOT NULL,
    to_user_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    group_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    message_type TINYINT NOT NULL DEFAULT 1,
    content TEXT NOT NULL,
    status TINYINT NOT NULL DEFAULT 1,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    INDEX idx_conversation_time (conversation_id, created_at),
    INDEX idx_from_user_time (from_user_id, created_at),
    INDEX idx_to_user_time (to_user_id, created_at),
    INDEX idx_group_time (group_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS offline_messages (
    id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT UNSIGNED NOT NULL,
    message_id BIGINT UNSIGNED NOT NULL,
    payload TEXT NOT NULL,
    status TINYINT NOT NULL DEFAULT 0,
    created_at BIGINT NOT NULL,
    updated_at BIGINT NOT NULL,
    UNIQUE KEY uk_user_message (user_id, message_id),
    INDEX idx_user_status_time (user_id, status, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

INSERT INTO users(username, password_hash, nickname, avatar, created_at, updated_at)
VALUES('nebula_test', 'mock_hash', 'Nebula Test', '', UNIX_TIMESTAMP()*1000, UNIX_TIMESTAMP()*1000)
ON DUPLICATE KEY UPDATE nickname=VALUES(nickname), updated_at=VALUES(updated_at);
