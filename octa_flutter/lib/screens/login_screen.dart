import 'package:flutter/foundation.dart';
   import 'package:flutter/material.dart';
   import 'package:provider/provider.dart';
   import 'signup_screen.dart';
   import 'dashboard_screen.dart';
   import '../services/auth_service.dart';

   class LoginParams {
     final String username;
     final String password;

     LoginParams(this.username, this.password);
   }

   Future<bool> _performLogin(LoginParams params) async {
     final authService = AuthService();
     return authService.login(params.username, params.password);
   }

   class LoginScreen extends StatefulWidget {
     const LoginScreen({Key? key}) : super(key: key);

     @override
     _LoginScreenState createState() => _LoginScreenState();
   }

   class _LoginScreenState extends State<LoginScreen> {
     final _usernameController = TextEditingController();
     final _passwordController = TextEditingController();
     bool _isLoading = false;

     void _login() async {
       setState(() => _isLoading = true);
       try {
         final success = await compute(
           _performLogin,
           LoginParams(
             _usernameController.text.trim(),
             _passwordController.text.trim(),
           ),
         );
         setState(() => _isLoading = false);

         if (success) {
           Navigator.pushReplacement(
             context,
             MaterialPageRoute(builder: (context) => const DashboardScreen()),
           );
         } else {
           ScaffoldMessenger.of(context).showSnackBar(
             const SnackBar(content: Text('Invalid credentials')),
           );
         }
       } catch (e) {
         setState(() => _isLoading = false);
         ScaffoldMessenger.of(context).showSnackBar(
           SnackBar(content: Text('Failed to connect to server: $e')),
         );
       }
     }

     @override
     Widget build(BuildContext context) {
       return Scaffold(
         body: Container(
           decoration: BoxDecoration(
             gradient: LinearGradient(
               begin: Alignment.topCenter,
               end: Alignment.bottomCenter,
               colors: [Colors.blue.shade800, Colors.blue.shade200],
             ),
           ),
           child: Center(
             child: SingleChildScrollView(
               padding: const EdgeInsets.all(24.0),
               child: Card(
                 elevation: 8,
                 shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
                 child: Padding(
                   padding: const EdgeInsets.all(24.0),
                   child: Column(
                     mainAxisSize: MainAxisSize.min,
                     children: [
                       Image.asset('assets/logo.png', height: 80),
                       const SizedBox(height: 16),
                       const Text(
                         'Octa-Solar',
                         style: TextStyle(
                           fontSize: 28,
                           fontWeight: FontWeight.bold,
                           color: Colors.blue,
                         ),
                       ),
                       const SizedBox(height: 24),
                       TextField(
                         controller: _usernameController,
                         decoration: InputDecoration(
                           labelText: 'Username',
                           border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                           prefixIcon: Icon(Icons.person),
                         ),
                       ),
                       const SizedBox(height: 16),
                       TextField(
                         controller: _passwordController,
                         obscureText: true,
                         decoration: InputDecoration(
                           labelText: 'Password',
                           border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                           prefixIcon: Icon(Icons.lock),
                         ),
                       ),
                       const SizedBox(height: 24),
                       _isLoading
                           ? const CircularProgressIndicator()
                           : ElevatedButton(
                               onPressed: _login,
                               style: ElevatedButton.styleFrom(
                                 minimumSize: const Size(double.infinity, 50),
                                 shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                               ),
                               child: const Text('Login', style: TextStyle(fontSize: 16)),
                             ),
                       const SizedBox(height: 16),
                       TextButton(
                         onPressed: () => Navigator.push(
                           context,
                           MaterialPageRoute(builder: (context) => const SignupScreen()),
                         ),
                         child: const Text('Don’t have an account? Sign up'),
                       ),
                     ],
                   ),
                 ),
               ),
             ),
           ),
         ),
       );
     }
   }
