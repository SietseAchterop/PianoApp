// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'main.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

Settings _$SettingsFromJson(Map<String, dynamic> json) => Settings(
      json['device'] as String,
      json['name'] as String,
      (json['settings'] as List<dynamic>)
          .map((e) => Position.fromJson(e as Map<String, dynamic>))
          .toList(),
    )..calibrated = json['calibrated'] as bool;

Map<String, dynamic> _$SettingsToJson(Settings instance) => <String, dynamic>{
      'device': instance.device,
      'name': instance.name,
      'calibrated': instance.calibrated,
      'settings': instance.settings,
    };

Position _$PositionFromJson(Map<String, dynamic> json) => Position(
      json['name'] as String,
      json['L'] as int,
      json['R'] as int,
    );

Map<String, dynamic> _$PositionToJson(Position instance) => <String, dynamic>{
      'name': instance.name,
      'L': instance.L,
      'R': instance.R,
    };
