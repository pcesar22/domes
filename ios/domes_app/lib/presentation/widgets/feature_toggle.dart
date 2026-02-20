/// Feature toggle switch widget.
library;

import 'package:domes_app/data/proto/generated/config.pbenum.dart';
import 'package:flutter/material.dart';

import '../../data/protocol/config_protocol.dart';

class FeatureToggle extends StatelessWidget {
  final AppFeatureState featureState;
  final ValueChanged<bool> onToggle;

  const FeatureToggle({
    super.key,
    required this.featureState,
    required this.onToggle,
  });

  @override
  Widget build(BuildContext context) {
    return SwitchListTile(
      title: Text(_featureName(featureState.feature)),
      subtitle: Text(_featureDescription(featureState.feature)),
      value: featureState.enabled,
      onChanged: onToggle,
      secondary: Icon(_featureIcon(featureState.feature)),
    );
  }

  String _featureName(Feature feature) {
    if (feature == Feature.FEATURE_LED_EFFECTS) return 'LED Effects';
    if (feature == Feature.FEATURE_BLE_ADVERTISING) return 'BLE Advertising';
    if (feature == Feature.FEATURE_WIFI) return 'WiFi';
    if (feature == Feature.FEATURE_ESP_NOW) return 'ESP-NOW';
    if (feature == Feature.FEATURE_TOUCH) return 'Touch';
    if (feature == Feature.FEATURE_HAPTIC) return 'Haptic';
    if (feature == Feature.FEATURE_AUDIO) return 'Audio';
    return 'Feature ${feature.value}';
  }

  String _featureDescription(Feature feature) {
    if (feature == Feature.FEATURE_LED_EFFECTS) return 'LED animations and effects';
    if (feature == Feature.FEATURE_BLE_ADVERTISING) return 'Bluetooth Low Energy advertising';
    if (feature == Feature.FEATURE_WIFI) return 'WiFi connectivity';
    if (feature == Feature.FEATURE_ESP_NOW) return 'ESP-NOW peer-to-peer';
    if (feature == Feature.FEATURE_TOUCH) return 'Touch sensing';
    if (feature == Feature.FEATURE_HAPTIC) return 'Haptic feedback';
    if (feature == Feature.FEATURE_AUDIO) return 'Audio output';
    return '';
  }

  IconData _featureIcon(Feature feature) {
    if (feature == Feature.FEATURE_LED_EFFECTS) return Icons.lightbulb;
    if (feature == Feature.FEATURE_BLE_ADVERTISING) return Icons.bluetooth;
    if (feature == Feature.FEATURE_WIFI) return Icons.wifi;
    if (feature == Feature.FEATURE_ESP_NOW) return Icons.cell_tower;
    if (feature == Feature.FEATURE_TOUCH) return Icons.touch_app;
    if (feature == Feature.FEATURE_HAPTIC) return Icons.vibration;
    if (feature == Feature.FEATURE_AUDIO) return Icons.volume_up;
    return Icons.help_outline;
  }
}
