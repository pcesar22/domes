import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'presentation/screens/home_screen.dart';
import 'presentation/theme/app_theme.dart';

void main() {
  runApp(const ProviderScope(child: DomesApp()));
}

class DomesApp extends StatelessWidget {
  const DomesApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'DOMES',
      theme: AppTheme.darkTheme,
      home: const HomeScreen(),
      debugShowCheckedModeBanner: false,
    );
  }
}
