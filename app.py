import mysql.connector
from flask import Flask, request, jsonify, send_from_directory
import os
from datetime import datetime, timedelta
import bcrypt
from flask_cors import CORS
import logging

# Configure logging
logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')

app = Flask(__name__, static_folder='static')
CORS(app, resources={r"/*": {"origins": "*"}})  # Explicitly allow all routes

db_config = {
    'host': 'localhost',
    'user': 'adminuser',
    'password': 'strongpassword',
    'database': 'octa',
    'charset': 'utf8mb4',  # Changed from utf8mb3
    'collation': 'utf8mb4_general_ci'
}

def get_db_connection():
    try:
        conn = mysql.connector.connect(**db_config)
        logging.debug("Database connection established")
        return conn
    except mysql.connector.Error as e:
        logging.error(f"Database connection failed: {str(e)}")
        raise

@app.route('/')
def welcome():
    logging.debug("Serving welcome page")
    return '''
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Octa-Solar Dashboard</title>
        <style>
            body { font-family: Arial, sans-serif; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f0f0; }
            .container { text-align: center; background-color: #fff; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }
            h1 { color: #007bff; }
            a { display: inline-block; margin-top: 20px; padding: 10px 20px; background-color: #007bff; color: #fff; text-decoration: none; border-radius: 5px; }
            a:hover { background-color: #0056b3; }
            p { margin: 10px 0; }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>Welcome to Octa-Solar</h1>
            <p>Access the Octa-Solar dashboard through our app:</p>
            <p><strong>For Web Users:</strong></p>
            <a href="/web/">Open Octa-Solar Web App</a>
            <p><strong>For Android Users:</strong> Install the Octa-Solar app on your device. If you don't have the APK, contact the administrator.</p>
            <p><strong>For Desktop Users:</strong> Download the desktop app for your platform (Linux, macOS, or Windows) from the administrator.</p>
        </div>
    </body>
    </html>
    '''

@app.route('/web/')
def serve_flutter_index():
    logging.debug("Serving index.html from octa_flutter/build/web")
    return send_from_directory('octa_flutter/build/web', 'index.html')

@app.route('/web/<path:path>')
def serve_flutter_web(path):
    logging.debug(f"Serving {path} from octa_flutter/build/web")
    return send_from_directory('octa_flutter/build/web', path)

@app.route('/update', methods=['POST'])
def update():
    data = request.get_json()
    logging.debug(f"Received update data: {data}")
    if not data:
        return jsonify({'error': 'No data provided'}), 400

    try:
        conn = get_db_connection()
        cursor = conn.cursor()

        voltage = data.get('voltage')
        current = data.get('current')
        tds = data.get('tds')
        flow = data.get('flow')
        light = data.get('light')
        switch = data.get('switch', 0)
        pump_id = data.get('pump_id', 1)

        inserted = 0
        if voltage is not None and voltage > 0:
            cursor.execute('INSERT INTO sensors (timestamp, value) VALUES (%s, %s)', (datetime.now(), voltage))
            inserted += 1
        if current is not None and current >= 0:
            cursor.execute('INSERT INTO sensors (timestamp, value) VALUES (%s, %s)', (datetime.now(), current))
            inserted += 1
        if tds is not None and tds >= 0:
            cursor.execute('INSERT INTO sensors (timestamp, value) VALUES (%s, %s)', (datetime.now(), tds))
            inserted += 1
        if flow is not None and flow >= 0:
            cursor.execute('INSERT INTO sensors (timestamp, value) VALUES (%s, %s)', (datetime.now(), flow))
            inserted += 1
        if light is not None and light >= 0:
            cursor.execute('INSERT INTO sensors (timestamp, value) VALUES (%s, %s)', (datetime.now(), light))
            inserted += 1

        cursor.execute('SELECT state, timestamp FROM pump_states WHERE pump_id = %s ORDER BY timestamp DESC LIMIT 1', (pump_id,))
        result = cursor.fetchone()
        current_state = result[0] if result else switch
        state_timestamp = result[1] if result else datetime.now()
        
        if (datetime.now() - state_timestamp).total_seconds() < 10:
            cursor.execute('INSERT INTO pump_states (timestamp, state, pump_id) VALUES (%s, %s, %s)', (datetime.now(), current_state, pump_id))
        else:
            current_state = switch
            cursor.execute('INSERT INTO pump_states (timestamp, state, pump_id) VALUES (%s, %s, %s)', (datetime.now(), current_state, pump_id))

        conn.commit()
        logging.debug(f"Inserted {inserted} sensor records, pump state for pump_id {pump_id}: {current_state}")

        return jsonify({
            'status': 'success',
            'pump_states': [{'pump_id': pump_id, 'state': current_state, 'timestamp': str(state_timestamp)}]
        }), 200
    except mysql.connector.Error as e:
        logging.error(f"Update database error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    except Exception as e:
        logging.error(f"Update error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    finally:
        if 'cursor' in locals():
            cursor.close()
        if 'conn' in locals():
            conn.close()

@app.route('/api/pump_state', methods=['GET'])
def get_pump_state():
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute('SELECT state, pump_id, timestamp FROM pump_states WHERE pump_id = 1 ORDER BY timestamp DESC LIMIT 1')
        result = cursor.fetchone()
        if result and (datetime.now() - result[2]).total_seconds() < 10:
            return jsonify({
                'status': 'success',
                'pump_state': {'pump_id': result[1], 'state': result[0], 'timestamp': str(result[2])}
            }), 200
        else:
            return jsonify({'status': 'error', 'message': 'No recent pump state found'}), 404
    except mysql.connector.Error as e:
        logging.error(f"Pump state database error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    except Exception as e:
        logging.error(f"Pump state error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    finally:
        if 'cursor' in locals():
            cursor.close()
        if 'conn' in locals():
            conn.close()

@app.route('/api/login', methods=['POST'])
def login():
    data = request.get_json()
    logging.debug(f"Raw login request: {data}")
    if not data or 'username' not in data or 'password' not in data:
        logging.warning("Missing username or password")
        return jsonify({'status': 'error', 'message': 'Missing username or password'}), 400

    username = data['username'].lower().strip()  # Normalize username
    password = data['password'].strip()  # Keep password case-sensitive
    logging.debug(f"Normalized login: username={username}, password={password}, password_bytes={password.encode('utf-8')}")

    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute('SELECT password FROM users WHERE username = %s', (username,))
        result = cursor.fetchone()
        logging.debug(f"DB result for {username}: {result}")

        if result:
            stored_hash = result[0]
            if isinstance(stored_hash, str):
                stored_hash = stored_hash.encode('utf-8')
            if bcrypt.checkpw(password.encode('utf-8'), stored_hash):
                logging.debug(f"Login successful for {username}")
                return jsonify({'status': 'success', 'message': 'Login successful'}), 200
            else:
                logging.debug(f"Password mismatch for {username}, input_bytes={password.encode('utf-8')}, stored_hash={stored_hash}")
                return jsonify({'status': 'error', 'message': 'Invalid credentials'}), 401
        else:
            logging.debug(f"User {username} not found")
            return jsonify({'status': 'error', 'message': 'Invalid credentials'}), 401
    except mysql.connector.Error as e:
        logging.error(f"Login database error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    except Exception as e:
        logging.error(f"Login error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    finally:
        if 'cursor' in locals():
            cursor.close()
        if 'conn' in locals():
            conn.close()

@app.route('/api/signup', methods=['POST'])
def signup():
    data = request.get_json()
    logging.debug(f"Raw signup request: {data}")
    if not data or 'username' not in data or 'password' not in data or 'email' not in data:
        logging.warning("Missing required fields")
        return jsonify({'status': 'error', 'message': 'Missing required fields'}), 400

    username = data['username'].lower().strip()  # Normalize username
    password = data['password'].strip()  # Keep password case-sensitive
    email = data['email'].lower().strip()  # Normalize email
    logging.debug(f"Normalized signup: username={username}, password={password}, email={email}, password_bytes={password.encode('utf-8')}")

    try:
        hashed_password = bcrypt.hashpw(password.encode('utf-8'), bcrypt.gensalt())
        logging.debug(f"Hashed password for {username}: {hashed_password}")

        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute('INSERT INTO users (username, password, email) VALUES (%s, %s, %s)', (username, hashed_password, email))
        conn.commit()
        logging.debug(f"User {username} committed to database")
        return jsonify({'status': 'success', 'message': 'User created'}), 201
    except mysql.connector.Error as e:
        if e.errno == 1062:
            logging.warning(f"Duplicate entry: {str(e)}")
            return jsonify({'status': 'error', 'message': 'Username or email already exists'}), 409
        logging.error(f"Signup database error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    except Exception as e:
        logging.error(f"Signup error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    finally:
        if 'cursor' in locals():
            cursor.close()
        if 'conn' in locals():
            conn.close()

@app.route('/api/sensors', methods=['GET'])
def get_sensors():
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute('SELECT timestamp, value FROM sensors ORDER BY timestamp DESC LIMIT 5')
        sensors = cursor.fetchall()

        query = (
            "SELECT ps.timestamp, ps.state, ps.pump_id "
            "FROM pump_states ps "
            "INNER JOIN ("
            "    SELECT pump_id, MAX(timestamp) as max_timestamp "
            "    FROM pump_states "
            "    GROUP BY pump_id"
            ") latest ON ps.pump_id = latest.pump_id AND ps.timestamp = latest.max_timestamp "
            "WHERE ps.timestamp > CURRENT_TIMESTAMP - INTERVAL 10 SECOND"
        )
        logging.debug(f"Executing pump_states query: {query}")
        cursor.execute(query)
        pump_states = cursor.fetchall()

        logging.debug(f"Sensors fetched: {len(sensors)}, Pump states: {len(pump_states)}")
        return jsonify({
            'sensors': [{'timestamp': str(row[0]), 'value': row[1]} for row in sensors],
            'pump_states': [
                {'timestamp': str(row[0]), 'state': row[1], 'pump_id': row[2]} for row in pump_states
            ]
        }), 200
    except mysql.connector.Error as e:
        logging.error(f"Sensors database error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    except Exception as e:
        logging.error(f"Sensors error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    finally:
        if 'cursor' in locals():
            cursor.close()
        if 'conn' in locals():
            conn.close()

@app.route('/api/switch', methods=['POST'])
def control_switch():
    data = request.get_json()
    logging.debug(f"Switch data: {data}")
    if not data or 'state' not in data or 'pump_id' not in data:
        logging.warning("Missing state or pump_id")
        return jsonify({'status': 'error', 'message': 'Missing state or pump_id'}), 400

    state = data['state']
    pump_id = data['pump_id']

    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute('INSERT INTO pump_states (timestamp, state, pump_id) VALUES (%s, %s, %s)', (datetime.now(), state, pump_id))
        conn.commit()
        logging.debug(f"Switch updated: pump_id={pump_id}, state={state}")
        return jsonify({'status': 'success', 'state': state, 'pump_id': pump_id}), 200
    except mysql.connector.Error as e:
        logging.error(f"Switch database error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    except Exception as e:
        logging.error(f"Switch error: {str(e)}")
        return jsonify({'error': str(e)}), 500
    finally:
        if 'cursor' in locals():
            cursor.close()
        if 'conn' in locals():
            conn.close()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
