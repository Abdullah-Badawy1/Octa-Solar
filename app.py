from flask import Flask, request, jsonify, render_template, redirect, url_for, session
from flask_sqlalchemy import SQLAlchemy
from flask_cors import CORS
from datetime import datetime, timedelta
import json
from werkzeug.security import generate_password_hash, check_password_hash
from functools import wraps
from config import Config

# Initialize Flask app
app = Flask(__name__)
app.config.from_object(Config)
CORS(app)

# Initialize SQLAlchemy
db = SQLAlchemy(app)

# Define Models
class SensorReading(db.Model):
    __tablename__ = 'sensor_readings'
    
    id = db.Column(db.Integer, primary_key=True)
    timestamp = db.Column(db.DateTime, default=datetime.utcnow)
    voltage = db.Column(db.Float, nullable=False)
    current = db.Column(db.Float, nullable=False)
    tds = db.Column(db.Float, nullable=False)
    flow_rate = db.Column(db.Float, nullable=False)
    total_liters = db.Column(db.Float, nullable=False)
    light = db.Column(db.Integer, nullable=False)
    relay_state = db.Column(db.Boolean, nullable=False)
    
    def to_dict(self):
        return {
            'id': self.id,
            'timestamp': self.timestamp.isoformat(),
            'voltage': self.voltage,
            'current': self.current,
            'tds': self.tds,
            'flow_rate': self.flow_rate,
            'total_liters': self.total_liters,
            'light': self.light,
            'relay_state': self.relay_state
        }

class User(db.Model):
    __tablename__ = 'users'
    
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(50), unique=True, nullable=False)
    password_hash = db.Column(db.String(100), nullable=False)
    email = db.Column(db.String(100), unique=True, nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    role = db.Column(db.String(10), default='user')
    
    def set_password(self, password):
        self.password_hash = generate_password_hash(password)
    
    def check_password(self, password):
        return check_password_hash(self.password_hash, password)

# Authentication Decorator
def login_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'user_id' not in session:
            return redirect(url_for('login', next=request.url))
        return f(*args, **kwargs)
    return decorated_function

# Global variable to store current relay state
current_relay_state = False

# Routes
@app.route('/')
def index():
    if 'user_id' in session:
        return redirect(url_for('dashboard'))
    return render_template('index.html')

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        
        user = User.query.filter_by(username=username).first()
        if user and user.check_password(password):
            session['user_id'] = user.id
            session['username'] = user.username
            session['role'] = user.role
            return redirect(url_for('dashboard'))
        
        return render_template('index.html', error='Invalid username or password')
    
    return render_template('index.html')

@app.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('index'))

@app.route('/dashboard')
@login_required
def dashboard():
    return render_template('dashboard.html')

# API Routes
@app.route('/api/readings', methods=['GET', 'POST'])
def api_readings():
    global current_relay_state
    
    if request.method == 'POST':
        data = request.json
        
        # Create new reading
        new_reading = SensorReading(
            voltage=data.get('voltage', 0),
            current=data.get('current', 0),
            tds=data.get('tds', 0),
            flow_rate=data.get('flow_rate', 0),
            total_liters=data.get('total_liters', 0),
            light=data.get('light', 0),
            relay_state=data.get('relay_state', False)
        )
        
        # Update current relay state
        current_relay_state = data.get('relay_state', current_relay_state)
        
        db.session.add(new_reading)
        db.session.commit()
        
        # Return current relay state for device to sync
        return jsonify({
            'status': 'success',
            'message': 'Reading saved successfully',
            'relay_command': current_relay_state
        })
    
    # Handle GET request
    time_range = request.args.get('range', 'day')
    
    # Calculate time filter based on range parameter
    now = datetime.utcnow()
    if time_range == 'hour':
        time_filter = now - timedelta(hours=1)
    elif time_range == 'day':
        time_filter = now - timedelta(days=1)
    elif time_range == 'week':
        time_filter = now - timedelta(weeks=1)
    elif time_range == 'month':
        time_filter = now - timedelta(days=30)
    else:
        time_filter = now - timedelta(days=1)  # Default to day
    
    # Query readings based on time filter
    readings = SensorReading.query.filter(SensorReading.timestamp >= time_filter).all()
    
    # Convert readings to dict and return as JSON
    return jsonify({
        'status': 'success',
        'count': len(readings),
        'data': [reading.to_dict() for reading in readings]
    })

@app.route('/api/readings/latest', methods=['GET'])
def api_latest_reading():
    latest_reading = SensorReading.query.order_by(SensorReading.timestamp.desc()).first()
    
    if latest_reading:
        return jsonify({
            'status': 'success',
            'data': latest_reading.to_dict()
        })
    
    return jsonify({
        'status': 'error',
        'message': 'No readings found'
    }), 404

@app.route('/api/control/relay', methods=['POST'])
@login_required
def api_control_relay():
    global current_relay_state
    
    data = request.json
    new_state = data.get('state', False)
    
    # Update current relay state
    current_relay_state = new_state
    
    return jsonify({
        'status': 'success',
        'message': f'Relay state set to {current_relay_state}',
        'relay_state': current_relay_state
    })

if __name__ == '__main__':
    with app.app_context():
        db.create_all()  # Create all tables if they don't exist
    app.run(host='0.0.0.0', port=5000, debug=True)
