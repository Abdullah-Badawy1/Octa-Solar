import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class AuthService {
  static String get baseUrl {
    if (kIsWeb) {
      return 'http://192.168.205.6:5000';
    } else {
      return 'http://192.168.205.6:5000';
    }
  }

  // Retry mechanism for HTTP requests
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
        return response; // Success, return the response
      } catch (e) {
        retryCount++;
        if (retryCount == maxRetries) {
          print('Max retries reached. Giving up. Error: $e');
          rethrow; // Rethrow the error after max retries
        }
        print('Request failed (attempt $retryCount/$maxRetries). Retrying in ${delay.inSeconds} seconds... Error: $e');
        await Future.delayed(delay);
        delay = Duration(seconds: delay.inSeconds * 2); // Exponential backoff
      }
    }
    throw Exception('Unexpected error in retry logic');
  }

  Future<bool> signup(String email, String password) async {
    try {
      print('Attempting to sign up at $baseUrl/signup');
      final response = await _retryRequest(() => http.post(
        Uri.parse('$baseUrl/signup'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'email': email, 'password': password}),
      ));

      print('Signup response: ${response.statusCode} - ${response.body}');
      if (response.statusCode == 201) {
        return true;
      }
      return false;
    } catch (e) {
      print('Signup error: $e');
      return false;
    }
  }

  Future<bool> login(String email, String password) async {
    try {
      print('Attempting to login at $baseUrl/login');
      final response = await _retryRequest(() => http.post(
        Uri.parse('$baseUrl/login'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'email': email, 'password': password}),
      ));

      print('Login response: ${response.statusCode} - ${response.body}');
      if (response.statusCode == 200) {
        final prefs = await SharedPreferences.getInstance();
        await prefs.setString('email', email);
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
    return prefs.getString('email') != null;
  }

  Future<void> logout() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('email');
  }
}
