import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class AuthService with ChangeNotifier {
  static String get baseUrl {
    return 'http://192.168.223.6:5000';
  }

  Future<bool> signup(String username, String password, String email) async {
    try {
      final normalizedUsername = _sanitizeInput(username).toLowerCase();
      final normalizedPassword = _sanitizeInput(password);
      final normalizedEmail = _sanitizeInput(email).toLowerCase();
      print('Raw signup input: username="$username", password="$password", email="$email"');
      print('Normalized signup: username="$normalizedUsername", password="$normalizedPassword", email="$normalizedEmail"');
      print('Signup request to $baseUrl/api/signup');

      final response = await http.post(
        Uri.parse('$baseUrl/api/signup'),
        headers: {
          'Content-Type': 'application/json; charset=UTF-8',
          'Accept': 'application/json',
        },
        body: jsonEncode({
          'username': normalizedUsername,
          'password': normalizedPassword,
          'email': normalizedEmail,
        }),
      );

      print('Signup response: ${response.statusCode} - ${response.body}');
      if (response.statusCode == 201) {
        notifyListeners();
        return true;
      }
      print('Signup failed: ${response.body}');
      return false;
    } catch (e) {
      print('Signup error: $e');
      return false;
    }
  }

  Future<bool> login(String username, String password) async {
    try {
      final normalizedUsername = _sanitizeInput(username).toLowerCase();
      final normalizedPassword = _sanitizeInput(password);
      print('Raw login input: username="$username", password="$password"');
      print('Normalized login: username="$normalizedUsername", password="$normalizedPassword"');
      print('Login request to $baseUrl/api/login');

      final response = await http.post(
        Uri.parse('$baseUrl/api/login'),
        headers: {
          'Content-Type': 'application/json; charset=UTF-8',
          'Accept': 'application/json',
        },
        body: jsonEncode({
          'username': normalizedUsername,
          'password': normalizedPassword,
        }),
      );

      print('Login response: ${response.statusCode} - ${response.body}');
      if (response.statusCode == 200) {
        final prefs = await SharedPreferences.getInstance();
        await prefs.setString('username', normalizedUsername);
        notifyListeners();
        return true;
      }
      print('Login failed: ${response.body}');
      return false;
    } catch (e) {
      print('Login error: $e');
      return false;
    }
  }

  String _sanitizeInput(String input) {
    String cleaned = input
        .replaceAll(RegExp(r'[^\x20-\x7E]'), '') // Keep ASCII printable
        .trim();
    print('Sanitized input: raw="$input", cleaned="$cleaned"');
    return cleaned;
  }

  Future<bool> isLoggedIn() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString('username') != null;
  }

  Future<void> logout() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('username');
    notifyListeners();
  }
}
