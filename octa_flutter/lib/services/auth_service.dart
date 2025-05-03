import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class AuthService with ChangeNotifier {
  static String get baseUrl {
    return 'http://192.168.205.6:5000';
  }

  Future<http.Response> _retryRequest(
    Future<http.Response> Function() request, {
    int maxRetries = 3,
    Duration initialDelay = const Duration(seconds: 2),
  }) async {
    int retryCount = 0;
    Duration delay = initialDelay;

    while (retryCount < maxRetries) {
      try {
        final response = await request();
        return response;
      } catch (e) {
        retryCount++;
        if (retryCount == maxRetries) {
          print('Max retries reached. Error: $e');
          rethrow;
        }
        print('Request failed (attempt $retryCount/$maxRetries). Retrying in ${delay.inSeconds}s... Error: $e');
        await Future.delayed(delay);
        delay = Duration(seconds: delay.inSeconds * 2);
      }
    }
    throw Exception('Unexpected error in retry logic');
  }

  Future<bool> signup(String username, String password, String email) async {
    try {
      print('Attempting signup at $baseUrl/api/signup');
      final response = await _retryRequest(() => http.post(
            Uri.parse('$baseUrl/api/signup'),
            headers: {'Content-Type': 'application/json'},
            body: jsonEncode({
              'username': username,
              'password': password,
              'email': email,
            }),
          ));

      print('Signup response: ${response.statusCode} - ${response.body}');
      if (response.statusCode == 201) {
        notifyListeners();
        return true;
      }
      return false;
    } catch (e) {
      print('Signup error: $e');
      return false;
    }
  }

  Future<bool> login(String username, String password) async {
    try {
      print('Attempting login at $baseUrl/api/login');
      final response = await _retryRequest(() => http.post(
            Uri.parse('$baseUrl/api/login'),
            headers: {'Content-Type': 'application/json'},
            body: jsonEncode({
              'username': username,
              'password': password,
            }),
          ));

      print('Login response: ${response.statusCode} - ${response.body}');
      if (response.statusCode == 200) {
        final prefs = await SharedPreferences.getInstance();
        await prefs.setString('username', username);
        notifyListeners();
        return true;
      }
      return false;
    } catch (e) {
      print('Login error: $e');
      return false;
    }
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
