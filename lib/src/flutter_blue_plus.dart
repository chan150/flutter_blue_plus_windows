import 'package:flutter_blue_plus/flutter_blue_plus.dart' as BLE;

class FlutterBluePlus {
  static BLE.LogLevel get logLevel => BLE.FlutterBluePlus.logLevel;

  /// Checks whether the device allows Bluetooth for your app
  static Future<bool> get isAvailable => BLE.FlutterBluePlus.isAvailable;

  /// Return the friendly Bluetooth name of the local Bluetooth adapter
  static Future<String> get adapterName => BLE.FlutterBluePlus.adapterName;

  // returns whether we are scanning as a stream
  static Stream<bool> get isScanning => BLE.FlutterBluePlus.isScanning;

  // are we scanning right now?
  static bool get isScanningNow => BLE.FlutterBluePlus.isScanningNow;

  /// Turn on Bluetooth (Android only),
  static Future<void> turnOn({int timeout = 10}) =>
      BLE.FlutterBluePlus.turnOn(timeout: timeout);

  /// Returns a stream of List<ScanResult> results while a scan is in progress.
  /// - The list contains all the results since the scan started.
  /// - When a scan is first started, an empty list is emitted.
  /// - The returned stream is never closed.
  static Stream<List<BLE.ScanResult>> get scanResults =>
      BLE.FlutterBluePlus.scanResults;

  /// Gets the current state of the Bluetooth module
  static Stream<BLE.BluetoothAdapterState> get adapterState =>
      BLE.FlutterBluePlus.adapterState;

  /// Retrieve a list of connected devices
  /// - The list includes devices connected by other apps
  /// - You must call device.connect() before these devices can be used by FlutterBluePlus
  static Future<List<BLE.BluetoothDevice>> get connectedSystemDevices =>
      BLE.FlutterBluePlus.connectedSystemDevices;

  /// Retrieve a list of bonded devices (Android only)
  static Future<List<BLE.BluetoothDevice>> get bondedDevices =>
      BLE.FlutterBluePlus.bondedDevices;

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
  }) =>
      BLE.FlutterBluePlus.scan(
        scanMode: scanMode,
        withServices: withServices,
        macAddresses: macAddresses,
        timeout: timeout,
        allowDuplicates: allowDuplicates,
        androidUsesFineLocation: androidUsesFineLocation,
      );

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
  }) =>
      BLE.FlutterBluePlus.startScan(
        scanMode: scanMode,
        withServices: withServices,
        macAddresses: macAddresses,
        timeout: timeout,
        allowDuplicates: allowDuplicates,
        androidUsesFineLocation: androidUsesFineLocation,
      );

  /// Stops a scan for Bluetooth Low Energy devices
  static Future stopScan() => BLE.FlutterBluePlus.stopScan();

  /// Sets the internal FlutterBlue log level
  static void setLogLevel(BLE.LogLevel level, {color = true}) =>
      BLE.FlutterBluePlus.setLogLevel(
        level,
        color: color,
      );

  /// Turn off Bluetooth (Android only),
  @Deprecated('Deprecated in Android SDK 33 with no replacement')
  static Future<void> turnOff({int timeout = 10}) =>
      BLE.FlutterBluePlus.turnOff(timeout: timeout);

  /// Checks if Bluetooth functionality is turned on
  @Deprecated('Use adapterState.first == BluetoothAdapterState.on instead')
  static Future<bool> get isOn => BLE.FlutterBluePlus.isOn;

  @Deprecated('Use adapterName instead')
  static Future<String> get name => BLE.FlutterBluePlus.name;

  @Deprecated('Use adapterState instead')
  static Stream<BLE.BluetoothAdapterState> get state =>
      BLE.FlutterBluePlus.state;

  @Deprecated('No longer needed, remove this from your code')
  static void get instance => BLE.FlutterBluePlus.instance;

  @Deprecated('Use connectedSystemDevices instead')
  static Future<List<BLE.BluetoothDevice>> get connectedDevices =>
      BLE.FlutterBluePlus.connectedDevices;
}
