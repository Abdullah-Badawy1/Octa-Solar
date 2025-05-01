import mysql.connector
from mysql.connector import Error

def create_database():
    try:
        # Connect to MySQL server
        connection = mysql.connector.connect(
            host="localhost",
            user="root",
            password="rootedd"  # Change this to your MySQL root password
        )
        
        if connection.is_connected():
            cursor = connection.cursor()
            
            # Create database if it doesn't exist
            cursor.execute("CREATE DATABASE IF NOT EXISTS octa_solar_db")
            print("Database 'octa_solar_db' created or already exists.")
            
            # Use the database
            cursor.execute("USE octa_solar_db")
            
            # Create sensor_readings table
            cursor.execute("""
            CREATE TABLE IF NOT EXISTS sensor_readings (
                id INT AUTO_INCREMENT PRIMARY KEY,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                voltage FLOAT NOT NULL,
                current FLOAT NOT NULL,
                tds FLOAT NOT NULL,
                flow_rate FLOAT NOT NULL,
                total_liters FLOAT NOT NULL,
                light INT NOT NULL,
                relay_state BOOLEAN NOT NULL
            )
            """)
            print("Table 'sensor_readings' created or already exists.")
            
            # Create users table for basic authentication
            cursor.execute("""
            CREATE TABLE IF NOT EXISTS users (
                id INT AUTO_INCREMENT PRIMARY KEY,
                username VARCHAR(50) UNIQUE NOT NULL,
                password_hash VARCHAR(100) NOT NULL,
                email VARCHAR(100) UNIQUE NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                role ENUM('admin', 'user') DEFAULT 'user'
            )
            """)
            print("Table 'users' created or already exists.")
            
            # Insert default admin user if not exists
            cursor.execute("""
            INSERT IGNORE INTO users (username, password_hash, email, role)
            VALUES ('admin', '$2b$12$aBcDeFgHiJkLmNoPqRsTuVwXyZ123456', 'admin@octasolar.com', 'admin')
            """)
            print("Default admin user created if not already exists.")
            
            connection.commit()
    
    except Error as e:
        print(f"Error: {e}")
    
    finally:
        if connection.is_connected():
            cursor.close()
            connection.close()
            print("MySQL connection closed.")

if __name__ == "__main__":
    create_database()
