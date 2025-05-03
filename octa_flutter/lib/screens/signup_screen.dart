import 'package:flutter/foundation.dart';
   import 'package:flutter/material.dart';
   import 'package:provider/provider.dart';
   import '../services/auth_service.dart';
   import 'dashboard_screen.dart';

   class SignupParams {
     final String username;
     final String password;
     final String email;

     SignupParams(this.username, this.password, this.email);
   }

   Future<bool> _performSignup(SignupParams params) async {
     final authService = AuthService();
     return authService.signup(params.username, params.password, params.email);
   }

   class SignupScreen extends StatefulWidget {
     const SignupScreen({Key? key}) : super(key: key);

     @override
     _SignupScreenState createState() => _SignupScreenState();
   }

   class _SignupScreenState extends State<SignupScreen> {
     final _usernameController = TextEditingController();
     final _passwordController = TextEditingController();
     final _emailController = TextEditingController();
     bool _isLoading = false;

     void _signup() async {
       setState(() => _isLoading = true);
       try {
         final success = await compute(
           _performSignup,
           SignupParams(
             _usernameController.text.trim(),
             _passwordController.text.trim(),
             _emailController.text.trim(),
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
             const SnackBar(content: Text('Signup failed. Username or email may already exist.')),
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
                         'Sign Up',
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
                         controller: _emailController,
                         decoration: InputDecoration(
                           labelText: 'Email',
                           border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                           prefixIcon: Icon(Icons.email),
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
                               onPressed: _signup,
                               style: ElevatedButton.styleFrom(
                                 minimumSize: const Size(double.infinity, 50),
                                 shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                               ),
                               child: const Text('Sign Up', style: TextStyle(fontSize: 16)),
                             ),
                       const SizedBox(height: 16),
                       TextButton(
                         onPressed: () => Navigator.pop(context),
                         child: const Text('Already have an account? Login'),
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
