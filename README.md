octa_solar_monitor/
├── app.py                 # Flask application entry point
├── config.py              # Configuration file
├── requirements.txt       # Python dependencies
├── init_db.py             # Database initialization script
├── static/                # Static files
│   ├── css/
│   │   └── style.css
│   ├── js/
│   │   └── main.js
│   └── images/
│       └── logo.png
├── templates/             # HTML templates
│   ├── index.html
│   ├── base.html
│   └── dashboard.html
└── flutter_app/           # Flutter mobile application
    ├── pubspec.yaml
    ├── lib/
    │   ├── main.dart
    │   ├── models/
    │   │   └── sensor_reading.dart
    │   ├── screens/
    │   │   ├── home_screen.dart
    │   │   └── details_screen.dart
    │   └── services/
    │       └── api_service.dart
    └── assets/
        └── images/
            └── octa_solar_logo.png
