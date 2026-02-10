/// DOMES app theme.
library;

import 'package:flutter/material.dart';

class AppTheme {
  static const _domesBlue = Color(0xFF1E88E5);
  static const _domesGreen = Color(0xFF43A047);
  static const _domesOrange = Color(0xFFFF8F00);

  static ThemeData get darkTheme => ThemeData(
        brightness: Brightness.dark,
        colorSchemeSeed: _domesBlue,
        useMaterial3: true,
        appBarTheme: const AppBarTheme(centerTitle: true),
      );

  static const Color connectedColor = _domesGreen;
  static const Color disconnectedColor = Colors.grey;
  static const Color connectingColor = _domesOrange;
  static const Color errorColor = Colors.red;
}
