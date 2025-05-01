import mysql.connector

db_config = {
    'host': 'localhost',
    'user': 'adminuser',
    'password': 'strongpassword',
    'database': 'sensor_db',
    'charset': 'utf8mb3',  # Match MariaDB's charset
    'collation': 'utf8mb3_general_ci'  # Match server default
}

try:
    conn = mysql.connector.connect(**db_config)
    print("Connection successful!")
    # Check supported collations
    cursor = conn.cursor()
    cursor.execute("SHOW COLLATION")
    collations = cursor.fetchall()
    for collation in collations:
        print(collation)
    # Check server version
    cursor.execute("SELECT VERSION()")
    version = cursor.fetchone()
    print("MariaDB Version:", version[0])
    conn.close()
except Exception as e:
    print("Error:", e)
