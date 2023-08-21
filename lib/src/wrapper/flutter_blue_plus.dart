import 'dart:io';

import 'package:flutter_blue_plus/flutter_blue_plus.dart' hide FlutterBluePlus;
import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

class FlutterBluePlus {
  static LogLevel get logLevel {
    return FlutterBluePlus.logLevel;
  }

  /// Checks whether the device allows Bluetooth for your app
  static Future<bool> get isAvailable async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.isAvailable;
    return await FlutterBluePlus.isAvailable;
  }

  /// Return the friendly Bluetooth name of the local Bluetooth adapter
  static Future<String> get adapterName async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.adapterName;
    return await FlutterBluePlus.adapterName;
  }

  // returns whether we are scanning as a stream
  static Stream<bool> get isScanning {
    if (Platform.isWindows) return FlutterBluePlusWindows.isScanning;
    return FlutterBluePlus.isScanning;
  }

  // are we scanning right now?
  static bool get isScanningNow {
    if (Platform.isWindows) return FlutterBluePlusWindows.isScanningNow;
    return FlutterBluePlus.isScanningNow;
  }

  static Future<void> turnOn({int timeout = 10}) async {
    if (Platform.isWindows)
      return FlutterBluePlusWindows.turnOn(timeout: timeout);
    await FlutterBluePlus.turnOn(timeout: timeout);
  }

  /// Returns a stream of List<ScanResult> results while a scan is in progress.
  /// - The list contains all the results since the scan started.
  /// - When a scan is first started, an empty list is emitted.
  /// - The returned stream is never closed.
  static Stream<List<ScanResult>> get scanResults {
    if (Platform.isWindows) return FlutterBluePlusWindows.scanResults;
    return FlutterBluePlus.scanResults;
  }

  /// Gets the current state of the Bluetooth module
  static Stream<BluetoothAdapterState> get adapterState {
    if (Platform.isWindows) return FlutterBluePlusWindows.adapterState;
    return FlutterBluePlus.adapterState;
  }

  /// Retrieve a list of connected devices
  /// - The list includes devices connected by other apps
  /// - You must call device.connect() before these devices can be used by FlutterBluePlus
  static Future<List<BluetoothDevice>> get connectedSystemDevices async {
    if (Platform.isWindows)
      return await FlutterBluePlusWindows.connectedSystemDevices;
    return await FlutterBluePlus.connectedSystemDevices;
  }

  /// Retrieve a list of bonded devices (Android only)
  static Future<List<BluetoothDevice>> get bondedDevices async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.bondedDevices;
    return await FlutterBluePlus.bondedDevices;
  }

  /// Starts a scan for Bluetooth Low Energy devices and returns a stream
  /// of the [ScanResult] results as they are received.
  ///    - throws an exception if scanning is already in progress
  ///    - [timeout] calls stopScan after a specified duration
  ///    - [androidUsesFineLocation] requests ACCESS_FINE_LOCATION permission at runtime regardless
  ///    of Android version. On Android 11 and below (Sdk < 31), this permission is required
  ///    and therefore we will always request it. Your AndroidManifest.xml must match.
  static Stream<ScanResult> scan({
    ScanMode scanMode = ScanMode.lowLatency,
    List<Guid> withServices = const [],
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
    return FlutterBluePlus.scan(
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
  static Future<void> startScan({
    ScanMode scanMode = ScanMode.lowLatency,
    List<Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) async {
    if (Platform.isWindows) {
      await FlutterBluePlusWindows.startScan(
        scanMode: scanMode,
        withServices: withServices,
        macAddresses: macAddresses,
        timeout: timeout,
        allowDuplicates: allowDuplicates,
        androidUsesFineLocation: androidUsesFineLocation,
      );
      return;
    }
    await FlutterBluePlus.startScan(
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
    await FlutterBluePlus.stopScan();
  }

  /// Sets the internal FlutterBlue log level
  static void setLogLevel(LogLevel level, {color = true}) {
    if (Platform.isWindows) {
      FlutterBluePlusWindows.setLogLevel(
        level,
        color: color,
      );
      return;
    }
    FlutterBluePlus.setLogLevel(
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
    await FlutterBluePlus.turnOff(timeout: timeout);
  }

  /// Checks if Bluetooth functionality is turned on
  @Deprecated('Use adapterState.first == BluetoothAdapterState.on instead')
  static Future<bool> get isOn async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.isOn;
    return await FlutterBluePlus.isOn;
  }

  @Deprecated('Use adapterName instead')
  static Future<String> get name async => await adapterName;

  @Deprecated('Use adapterState instead')
  static Stream<BluetoothAdapterState> get state => adapterState;

  @Deprecated('No longer needed, remove this from your code')
  static void get instance => null;

  @Deprecated('Use connectedSystemDevices instead')
  static Future<List<BluetoothDevice>> get connectedDevices async =>
      await connectedSystemDevices;
}
