import 'dart:io';

import 'package:flutter_blue_plus/flutter_blue_plus.dart' as BLE;
import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

class FlutterBluePlus {
  static BLE.LogLevel get logLevel {
    return BLE.FlutterBluePlus.logLevel;
  }

  /// Checks whether the device allows Bluetooth for your app
  static Future<bool> get isAvailable async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.isAvailable;
    return await BLE.FlutterBluePlus.isAvailable;
  }

  /// Return the friendly Bluetooth name of the local Bluetooth adapter
  static Future<String> get adapterName async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.adapterName;
    return await BLE.FlutterBluePlus.adapterName;
  }

  // returns whether we are scanning as a stream
  static Stream<bool> get isScanning {
    if (Platform.isWindows) return FlutterBluePlusWindows.isScanning;
    return BLE.FlutterBluePlus.isScanning;
  }

  // are we scanning right now?
  static bool get isScanningNow {
    if (Platform.isWindows) return FlutterBluePlusWindows.isScanningNow;
    return BLE.FlutterBluePlus.isScanningNow;
  }

  static Future<void> turnOn({int timeout = 10}) async {
    if (Platform.isWindows)
      return FlutterBluePlusWindows.turnOn(timeout: timeout);
    await BLE.FlutterBluePlus.turnOn(timeout: timeout);
  }

  /// Returns a stream of List<ScanResult> results while a scan is in progress.
  /// - The list contains all the results since the scan started.
  /// - When a scan is first started, an empty list is emitted.
  /// - The returned stream is never closed.
  static Stream<List<BLE.ScanResult>> get scanResults {
    if (Platform.isWindows) return FlutterBluePlusWindows.scanResults;
    return BLE.FlutterBluePlus.scanResults;
  }

  /// Gets the current state of the Bluetooth module
  static Stream<BLE.BluetoothAdapterState> get adapterState {
    if (Platform.isWindows) return FlutterBluePlusWindows.adapterState;
    return BLE.FlutterBluePlus.adapterState;
  }

  /// Retrieve a list of connected devices
  /// - The list includes devices connected by other apps
  /// - You must call device.connect() before these devices can be used by FlutterBluePlus
  static Future<List<BLE.BluetoothDevice>> get connectedSystemDevices async {
    if (Platform.isWindows)
      return await FlutterBluePlusWindows.connectedSystemDevices;
    return await BLE.FlutterBluePlus.connectedSystemDevices;
  }

  /// Retrieve a list of bonded devices (Android only)
  static Future<List<BLE.BluetoothDevice>> get bondedDevices async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.bondedDevices;
    return await BLE.FlutterBluePlus.bondedDevices;
  }

  /// Starts a scan for Bluetooth Low Energy devices and returns a stream
  /// of the [ScanResult] results as they are received.
  ///    - throws an exception if scanning is already in progress
  ///    - [timeout] calls stopScan after a specified duration
  ///    - [androidUsesFineLocation] requests ACCESS_FINE_LOCATION permission at runtime regardless
  ///    of Android version. On Android 11 and below (Sdk < 31), this permission is required
  ///    and therefore we will always request it. Your AndroidManifest.xml must match.
  static Stream<BLE.ScanResult> scan({
    BLE.ScanMode scanMode = BLE.ScanMode.lowLatency,
    List<BLE.Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) {
    if (Platform.isWindows) {
      return FlutterBluePlusWindows.scan(
        scanMode: scanMode,
        withServices: withServices,
        macAddresses: macAddresses,
        timeout: timeout,
        allowDuplicates: allowDuplicates,
        androidUsesFineLocation: androidUsesFineLocation,
      );
    }
    return BLE.FlutterBluePlus.scan(
      scanMode: scanMode,
      withServices: withServices,
      macAddresses: macAddresses,
      timeout: timeout,
      allowDuplicates: allowDuplicates,
      androidUsesFineLocation: androidUsesFineLocation,
    );
  }

  /// Start a scan
  ///  - future completes when the scan is done.
  ///  - To observe the results live, listen to the [scanResults] stream.
  ///  - call [stopScan] to complete the returned future, or set [timeout]
  ///  - see [scan] documentation for more details
  static Future startScan({
    BLE.ScanMode scanMode = BLE.ScanMode.lowLatency,
    List<BLE.Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) async {
    if (Platform.isWindows) {
      return await FlutterBluePlusWindows.startScan(
        scanMode: scanMode,
        withServices: withServices,
        macAddresses: macAddresses,
        timeout: timeout,
        allowDuplicates: allowDuplicates,
        androidUsesFineLocation: androidUsesFineLocation,
      );
    }
    return await BLE.FlutterBluePlus.startScan(
      scanMode: scanMode,
      withServices: withServices,
      macAddresses: macAddresses,
      timeout: timeout,
      allowDuplicates: allowDuplicates,
      androidUsesFineLocation: androidUsesFineLocation,
    );
  }

  /// Stops a scan for Bluetooth Low Energy devices
  static Future<void> stopScan() async {
    if (Platform.isWindows) {
      await FlutterBluePlusWindows.stopScan();
      return;
    }
    await BLE.FlutterBluePlus.stopScan();
  }

  /// Sets the internal FlutterBlue log level
  static void setLogLevel(BLE.LogLevel level, {color = true}) {
    if (Platform.isWindows) {
      FlutterBluePlusWindows.setLogLevel(
        level,
        color: color,
      );
      return;
    }
    BLE.FlutterBluePlus.setLogLevel(
      level,
      color: color,
    );
  }

  @Deprecated('Deprecated in Android SDK 33 with no replacement')
  static Future<void> turnOff({int timeout = 10}) async {
    if (Platform.isWindows) {
      FlutterBluePlusWindows.turnOff(timeout: timeout);
      return;
    }
    await BLE.FlutterBluePlus.turnOff(timeout: timeout);
  }

  /// Checks if Bluetooth functionality is turned on
  @Deprecated('Use adapterState.first == BluetoothAdapterState.on instead')
  static Future<bool> get isOn async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.isOn;
    return await BLE.FlutterBluePlus.isOn;
  }

  @Deprecated('Use adapterName instead')
  static Future<String> get name async => await adapterName;

  @Deprecated('Use adapterState instead')
  static Stream<BLE.BluetoothAdapterState> get state => adapterState;

  @Deprecated('No longer needed, remove this from your code')
  static void get instance => null;

  @Deprecated('Use connectedSystemDevices instead')
  static Future<List<BLE.BluetoothDevice>> get connectedDevices async =>
      await connectedSystemDevices;
}
