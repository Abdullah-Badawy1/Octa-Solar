import 'package:flutter/material.dart';
import 'package:octa_solar/screens/login_screen.dart';
import 'package:octa_solar/screens/home_screen.dart';
import 'package:octa_solar/services/auth_service.dart';

void main() {
  runApp(const OctaSolarApp());
}

class OctaSolarApp extends StatelessWidget {
  const OctaSolarApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Octa-Solar',
      theme: ThemeData(
        primarySwatch: Colors.blue,
        visualDensity: VisualDensity.adaptivePlatformDensity,
      ),
      home: FutureBuilder<bool>(
        future: AuthService().isLoggedIn(),
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          }
          if (snapshot.data == true) {
            return const HomeScreen();
          }
          return const LoginScreen();
        },
      ),
    );
  }
}
