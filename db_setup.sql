CREATE DATABASE octa CHARACTER SET utf8mb3 COLLATE utf8mb3_general_ci;
CREATE USER 'adminuser'@'localhost' IDENTIFIED BY 'strongpassword';
GRANT ALL PRIVILEGES ON octa.* TO 'adminuser'@'localhost';
FLUSH PRIVILEGES;
USE octa;
CREATE TABLE sensors (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME NOT NULL,
    value FLOAT NOT NULL,
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB;
CREATE TABLE pump_states (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME NOT NULL,
    state TINYINT NOT NULL,
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB;
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(255) NOT NULL,
    email VARCHAR(100) NOT NULL UNIQUE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;
INSERT INTO users (username, password, email) 
VALUES ('adminuser', '$2y$10$examplehashedpassword1234567890', 'admin@octasolar.com');
