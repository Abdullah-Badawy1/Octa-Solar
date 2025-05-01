from flask import Flask, request, jsonify, render_template
from flask_cors import CORS  # Import CORS
import mysql.connector
from datetime import datetime
import hashlib

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

# MySQL configuration
db_config = {
    'host': 'localhost',
    'user': 'adminuser',
    'password': 'strongpassword',
    'database': 'sensor_db',
    'charset': 'utf8mb3',
    'collation': 'utf8mb3_general_ci'
}

# Connect to MySQL
def get_db_connection():
    return mysql.connector.connect(**db_config)

# Hash password for security
def hash_password(password):
    return hashlib.sha256(password.encode()).hexdigest()

# Signup endpoint
@app.route('/signup', methods=['POST'])
def signup():
    data = request.get_json()
    email = data.get('email')
    password = hash_password(data.get('password'))

    conn = get_db_connection()
    cursor = conn.cursor()
    
    # Check if user exists
    cursor.execute("SELECT * FROM users WHERE email = %s", (email,))
    if cursor.fetchone():
        cursor.close()
        conn.close()
        return jsonify({'status': 'error', 'message': 'User already exists'}), 400

    # Insert new user
    cursor.execute("INSERT INTO users (email, password) VALUES (%s, %s)", (email, password))
    conn.commit()
    cursor.close()
    conn.close()
    return jsonify({'status': 'success', 'message': 'User created'}), 201

# Login endpoint
@app.route('/login', methods=['POST'])
def login():
    data = request.get_json()
    email = data.get('email')
    password = hash_password(data.get('password'))

    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM users WHERE email = %s AND password = %s", (email, password))
    user = cursor.fetchone()
    cursor.close()
    conn.close()

    if user:
        return jsonify({'status': 'success', 'message': 'Login successful'}), 200
    return jsonify({'status': 'error', 'message': 'Invalid credentials'}), 401

# Existing routes...
@app.route('/')
def index():
    return "Welcome to Octa-Solar Sensor Dashboard"

@app.route('/test')
def test():
    return render_template('test.html')

@app.route('/update', methods=['POST'])
def update():
    data = request.get_json()
    conn = get_db_connection()
    cursor = conn.cursor()
    
    query = """INSERT INTO sensor_data (voltage, current, tds, flow, liters, light, switch_state)
               VALUES (%s, %s, %s, %s, %s, %s, %s)"""
    values = (data['voltage'], data['current'], data['tds'], data['flow'], data['liters'], data['light'], data['switch'])
    cursor.execute(query, values)
    conn.commit()

    switch_response = request.args.get('switch', 'no_change')
    if switch_response == 'on':
        response = 'switch_on'
    elif switch_response == 'off':
        response = 'switch_off'
    else:
        response = 'no_change'

    cursor.close()
    conn.close()
    return jsonify({'status': 'success', 'switch': response})

@app.route('/data', methods=['GET'])
def get_data():
    conn = get_db_connection()
    cursor = conn.cursor(dictionary=True)
    cursor.execute("SELECT * FROM sensor_data ORDER BY timestamp DESC LIMIT 10")
    data = cursor.fetchall()
    cursor.close()
    conn.close()
    return jsonify(data)

@app.route('/switch', methods=['POST'])
def control_switch():
    state = request.json.get('state')
    return jsonify({'switch': 'on' if state == 1 else 'off'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
