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
    return switch (feature) {
      Feature.FEATURE_LED_EFFECTS => 'LED Effects',
      Feature.FEATURE_BLE_ADVERTISING => 'BLE Advertising',
      Feature.FEATURE_WIFI => 'WiFi',
      Feature.FEATURE_ESP_NOW => 'ESP-NOW',
      Feature.FEATURE_TOUCH => 'Touch',
      Feature.FEATURE_HAPTIC => 'Haptic',
      Feature.FEATURE_AUDIO => 'Audio',
      _ => 'Unknown',
    };
  }

  String _featureDescription(Feature feature) {
    return switch (feature) {
      Feature.FEATURE_LED_EFFECTS => 'LED animations and effects',
      Feature.FEATURE_BLE_ADVERTISING => 'Bluetooth Low Energy advertising',
      Feature.FEATURE_WIFI => 'WiFi connectivity',
      Feature.FEATURE_ESP_NOW => 'ESP-NOW peer-to-peer',
      Feature.FEATURE_TOUCH => 'Touch sensing',
      Feature.FEATURE_HAPTIC => 'Haptic feedback',
      Feature.FEATURE_AUDIO => 'Audio output',
      _ => '',
    };
  }

  IconData _featureIcon(Feature feature) {
    return switch (feature) {
      Feature.FEATURE_LED_EFFECTS => Icons.lightbulb,
      Feature.FEATURE_BLE_ADVERTISING => Icons.bluetooth,
      Feature.FEATURE_WIFI => Icons.wifi,
      Feature.FEATURE_ESP_NOW => Icons.cell_tower,
      Feature.FEATURE_TOUCH => Icons.touch_app,
      Feature.FEATURE_HAPTIC => Icons.vibration,
      Feature.FEATURE_AUDIO => Icons.volume_up,
      _ => Icons.help_outline,
    };
  }
}
