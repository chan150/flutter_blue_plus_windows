import 'dart:io';

import 'package:flutter_blue_plus/flutter_blue_plus.dart' as BLE;
import 'package:win_ble/win_ble.dart';
import 'package:win_ble/win_file.dart';

class FlutterBluePlusWindows {
  static bool _initialized = false;

  static Future<void> _initialize() async {
    if (_initialized) return;
    await WinBle.initialize(
      serverPath: await WinServer.path,
      enableLog: true,
    );
    _initialized = true;
  }

  static Future<bool> get isAvailable async {
    await _initialize();
    return true;
  }

  static Future<String> get adapterName async {
    await _initialize();
    return 'WindowsAdapter';
  }

  static Stream<bool> get isScanning async* {
    await _initialize();
    await for (final s in Stream<bool>.empty()) {
      yield s;
    }
  }

  static bool get isScanningNow {
    return false;
  }

  static Future<void> turnOn({int timeout = 10}) async {
    final a = await WinBle.getBluetoothState();
    print(a);
  }

  static Stream<List<BLE.ScanResult>> get scanResults async* {
    await _initialize();
    await for (final s in Stream<List<BLE.ScanResult>>.empty()) {
      yield s;
    }
  }

  static Stream<BLE.BluetoothAdapterState> get adapterState async* {
    await _initialize();
    await for (final s in Stream<BLE.BluetoothAdapterState>.empty()) {
      yield s;
    }
  }

  static Future<List<BLE.BluetoothDevice>> get connectedSystemDevices async {
    await _initialize();
    return [];
  }

  static Future<List<BLE.BluetoothDevice>> get bondedDevices async {
    await _initialize();
    return [];
  }

  static Stream<BLE.ScanResult> scan({
    BLE.ScanMode scanMode = BLE.ScanMode.lowLatency,
    List<BLE.Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) async* {
    await _initialize();
    await for (final s in Stream<BLE.ScanResult>.empty()) {
      yield s;
    }
  }

  static Future<void> startScan({
    BLE.ScanMode scanMode = BLE.ScanMode.lowLatency,
    List<BLE.Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) async {
    await _initialize();
  }

  /// Stops a scan for Bluetooth Low Energy devices
  static Future<void> stopScan() async {
    await _initialize();
  }

  BLE.LogLevel test = BLE.LogLevel.debug;

  /// Sets the internal FlutterBlue log level
  static void setLogLevel(BLE.LogLevel level, {color = true}) {
    return;
  }

  @Deprecated('Deprecated in Android SDK 33 with no replacement')
  static Future<void> turnOff({int timeout = 10}) async {
    await _initialize();
    await WinBle.updateBluetoothState(false);
  }

  /// Checks if Bluetooth functionality is turned on
  @Deprecated('Use adapterState.first == BluetoothAdapterState.on instead')
  static Future<bool> get isOn async {
    return true;
  }

  @Deprecated('Use adapterName instead')
  static Future<String> get name async => await adapterName;

  @Deprecated('Use adapterState instead')
  static Stream<BLE.BluetoothAdapterState> get state => adapterState;

  @Deprecated('No longer needed, remove this from your code')
  static void get instance => BLE.FlutterBluePlus.instance;

  @Deprecated('Use connectedSystemDevices instead')
  static Future<List<BLE.BluetoothDevice>> get connectedDevices async =>
      await connectedSystemDevices;
}
