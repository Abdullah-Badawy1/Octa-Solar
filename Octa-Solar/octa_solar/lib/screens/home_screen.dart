import 'package:flutter/material.dart';
import 'package:octa_solar/services/auth_service.dart';
import 'package:octa_solar/screens/login_screen.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final _authService = AuthService();

    return Scaffold(
      appBar: AppBar(
        title: const Text('Octa-Solar Dashboard'),
        backgroundColor: Colors.blue,
        actions: [
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: () async {
              await _authService.logout();
              Navigator.pushReplacement(
                context,
                MaterialPageRoute(builder: (context) => const LoginScreen()),
              );
            },
          ),
        ],
      ),
      body: const Center(
        child: Text(
          'Welcome to Octa-Solar!\nSensor data dashboard coming soon.',
          textAlign: TextAlign.center,
          style: TextStyle(fontSize: 20),
        ),
      ),
    );
  }
}
