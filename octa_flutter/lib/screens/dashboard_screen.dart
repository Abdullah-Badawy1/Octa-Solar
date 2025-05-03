import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';
import 'dart:async';
import 'package:provider/provider.dart';
import '../services/auth_service.dart';
import 'login_screen.dart';

class SensorData {
  final String timestamp;
  final double voltage;
  final double current;
  final double tds;
  final double flow;
  final int light;

  SensorData(this.timestamp, this.voltage, this.current, this.tds, this.flow, this.light);
}

class PumpState {
  final int id;
  bool state;

  PumpState(this.id, this.state);
}

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({Key? key}) : super(key: key);

  @override
  _DashboardScreenState createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  Map<String, dynamic> _sensorValues = {
    'voltage': {'raw': 0.0, 'readable': '0.0 V', 'corrected': 0.0, 'icon': Icons.electrical_services},
    'current': {'raw': 0.0, 'readable': '0.00 A', 'corrected': 0.0, 'icon': Icons.power},
    'tds': {'raw': 0.0, 'readable': '0.0 ppm', 'corrected': 0.0, 'icon': Icons.water_drop},
    'flow': {'raw': 0.0, 'readable': '0.0 L/min', 'corrected': 0.0, 'icon': Icons.waves},
    'light': {'raw': 0.0, 'readable': '0 lux', 'corrected': 0.0, 'icon': Icons.lightbulb},
  };
  List<PumpState> _pumps = [PumpState(1, false)];
  Timer? _timer;
  bool _isLoading = true;
  bool _isTimedOut = false;
  bool _isToggling = false;
  DateTime? _lastToggleTime;

  @override
  void initState() {
    super.initState();
    _fetchData();
    _timer = Timer.periodic(Duration(seconds: 2), (timer) => _fetchData());
    Future.delayed(Duration(seconds: 10), () {
      if (_isLoading && mounted) {
        setState(() {
          _isTimedOut = true;
        });
      }
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  Future<void> _fetchData() async {
    if (_isToggling) {
      print('Skipping fetchData during toggle');
      return;
    }

    try {
      final response = await http.get(Uri.parse('http://192.168.205.6:5000/api/sensors'));
      print('Fetch response: ${response.statusCode} - ${response.body}');

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        final sensors = data['sensors'] as List;
        final pumpStates = data['pump_states'] as List;

        setState(() {
          _isLoading = false;
          _isTimedOut = false;

          if (sensors.isNotEmpty) {
            _sensorValues['voltage'] = {
              'raw': sensors.length > 0 ? (sensors[0]['value']?.toDouble() ?? 0.0) : 0.0,
              'readable': '${sensors.length > 0 ? (sensors[0]['value']?.toDouble().toStringAsFixed(1) ?? '0.0') : '0.0'} V',
              'corrected': (sensors.length > 0 ? (sensors[0]['value']?.toDouble() ?? 0.0) : 0.0) * 1.0,
              'icon': Icons.electrical_services,
            };
            _sensorValues['current'] = {
              'raw': sensors.length > 1 ? (sensors[1]['value']?.toDouble() ?? 0.0) : 0.0,
              'readable': '${sensors.length > 1 ? (sensors[1]['value']?.toDouble().toStringAsFixed(2) ?? '0.00') : '0.00'} A',
              'corrected': (sensors.length > 1 ? (sensors[1]['value']?.toDouble() ?? 0.0) : 0.0) * 1.0,
              'icon': Icons.power,
            };
            _sensorValues['tds'] = {
              'raw': sensors.length > 2 ? (sensors[2]['value']?.toDouble() ?? 0.0) : 0.0,
              'readable': '${sensors.length > 2 ? (sensors[2]['value']?.toDouble().toStringAsFixed(1) ?? '0.0') : '0.0'} ppm',
              'corrected': (sensors.length > 2 ? (sensors[2]['value']?.toDouble() ?? 0.0) : 0.0) * 0.95,
              'icon': Icons.water_drop,
            };
            _sensorValues['flow'] = {
              'raw': sensors.length > 3 ? (sensors[3]['value']?.toDouble() ?? 0.0) : 0.0,
              'readable': '${sensors.length > 3 ? (sensors[3]['value']?.toDouble().toStringAsFixed(1) ?? '0.0') : '0.0'} L/min',
              'corrected': (sensors.length > 3 ? (sensors[3]['value']?.toDouble() ?? 0.0) : 0.0) * 1.0,
              'icon': Icons.waves,
            };
            _sensorValues['light'] = {
              'raw': sensors.length > 4 ? (sensors[4]['value']?.toDouble() ?? 0.0) : 0.0,
              'readable': '${sensors.length > 4 ? (sensors[4]['value']?.toInt() ?? 0) : 0} lux',
              'corrected': (sensors.length > 4 ? (sensors[4]['value']?.toDouble() ?? 0.0) : 0.0) * 1.0,
              'icon': Icons.lightbulb,
            };
          }

          // Update pumps with the latest state only
          if (pumpStates.isNotEmpty) {
            final latestState = pumpStates[0]; // Server returns only the latest
            final pumpId = latestState['pump_id'];
            final newState = latestState['state'] == 1;
            final pump = _pumps.firstWhere(
              (p) => p.id == pumpId,
              orElse: () => PumpState(pumpId, newState),
            );
            pump.state = newState;
            if (!_pumps.any((p) => p.id == pumpId)) {
              _pumps.add(pump);
            }
          }
        });
      } else {
        print('Fetch failed: ${response.statusCode}');
        setState(() {
          _isLoading = false;
        });
      }
    } catch (e) {
      print('Error fetching sensor data: $e');
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _togglePump(int pumpId, bool value) async {
    // Debounce button clicks (1 second)
    if (_lastToggleTime != null &&
        DateTime.now().difference(_lastToggleTime!).inMilliseconds < 1000) {
      print('Toggle ignored: too soon');
      return;
    }

    setState(() {
      _isToggling = true;
      _lastToggleTime = DateTime.now();
    });

    try {
      final switchResponse = await http.post(
        Uri.parse('http://192.168.205.6:5000/api/switch'),
        body: jsonEncode({'state': value ? 1 : 0, 'pump_id': pumpId}),
        headers: {'Content-Type': 'application/json'},
      );
      print('Toggle pump response: ${switchResponse.statusCode} - ${switchResponse.body}');

      if (switchResponse.statusCode == 200) {
        setState(() {
          _pumps.firstWhere((p) => p.id == pumpId).state = value;
          _isToggling = false;
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Pump $pumpId turned ${value ? "ON" : "OFF"}')),
        );
        print('Pump $pumpId toggled to ${value ? "ON" : "OFF"}');
      } else {
        print('Failed to toggle pump $pumpId: ${switchResponse.statusCode}');
        setState(() {
          _isToggling = false;
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to toggle Pump $pumpId')),
        );
      }
    } catch (e) {
      print('Error toggling pump $pumpId: $e');
      setState(() {
        _isToggling = false;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error toggling Pump $pumpId')),
      );
    }
  }

  Widget _buildSensorCard(String label, Map<String, dynamic> values, Color color) {
    return Card(
      elevation: 6,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
      child: Container(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            colors: [Colors.grey.shade100, Colors.white],
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
          ),
          borderRadius: BorderRadius.circular(12),
        ),
        padding: const EdgeInsets.all(16.0),
        child: Row(
          children: [
            Icon(values['icon'], color: color, size: 40),
            SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    label,
                    style: TextStyle(
                      fontSize: 20,
                      fontWeight: FontWeight.bold,
                      color: Colors.grey.shade800,
                    ),
                  ),
                  SizedBox(height: 8),
                  Text('Raw: ${values['raw']}', style: TextStyle(fontSize: 16, color: Colors.grey.shade600)),
                  Text('Readable: ${values['readable']}', style: TextStyle(fontSize: 16, fontWeight: FontWeight.w500)),
                  Text(
                    'Corrected: ${values['corrected']}',
                    style: TextStyle(fontSize: 16, color: color, fontWeight: FontWeight.bold),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Octa-Solar Dashboard'),
        backgroundColor: Colors.blueGrey,
        elevation: 4,
        actions: [
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: () async {
              await context.read<AuthService>().logout();
              Navigator.pushReplacement(
                context,
                MaterialPageRoute(builder: (context) => const LoginScreen()),
              );
            },
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Stack(
          children: [
            SingleChildScrollView(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Text(
                    'Sensor Readings',
                    style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.blueGrey),
                  ),
                  const SizedBox(height: 16),
                  _buildSensorCard('Voltage', _sensorValues['voltage']!, Colors.green),
                  _buildSensorCard('Current', _sensorValues['current']!, Colors.blue),
                  _buildSensorCard('TDS', _sensorValues['tds']!, Colors.orange),
                  _buildSensorCard('Flow Rate', _sensorValues['flow']!, Colors.purple),
                  _buildSensorCard('Light', _sensorValues['light']!, Colors.red),
                  const SizedBox(height: 24),
                  const Text(
                    'Pump Controls',
                    style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.blueGrey),
                  ),
                  const SizedBox(height: 16),
                  ..._pumps.map((pump) => Card(
                        elevation: 6,
                        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
                        child: ListTile(
                          leading: Icon(Icons.power_settings_new, color: pump.state ? Colors.green : Colors.grey),
                          title: Text('Pump ${pump.id}', style: TextStyle(fontSize: 18, fontWeight: FontWeight.w500)),
                          trailing: _isToggling
                              ? CircularProgressIndicator(strokeWidth: 2)
                              : Switch(
                                  value: pump.state,
                                  activeColor: Colors.green,
                                  onChanged: _isToggling ? null : (value) => _togglePump(pump.id, value),
                                ),
                        ),
                      )),
                ],
              ),
            ),
            if (_isLoading)
              Center(
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    CircularProgressIndicator(color: Colors.blueGrey),
                    SizedBox(height: 16),
                    Text(
                      _isTimedOut ? 'Waiting for data... (Taking longer than expected)' : 'Loading data...',
                      style: TextStyle(fontSize: 16, color: Colors.grey.shade700),
                    ),
                  ],
                ),
              ),
          ],
        ),
      ),
    );
  }
}
