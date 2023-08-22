import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';
import 'package:win_ble/win_ble.dart';
import 'package:win_ble/win_file.dart';

class FlutterBluePlusWindows {
  static bool _initialized = false;

  static Future<void> _initialize() async {
    if (_initialized) return;
    await WinBle.initialize(
      serverPath: await WinServer.path,
      enableLog: false,
    );
    _initialized = true;
  }

  static Future<bool> get isAvailable async {
    await _initialize();
    return true;
  }

  static Future<String> get adapterName async {
    await _initialize();
    return 'Windows';
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
    await _initialize();
    await WinBle.updateBluetoothState(true);
  }

  static Stream<List<ScanResult>> get scanResults async* {
    await _initialize();

    final List<ScanResult> list = [];

    await for (final s in WinBle.scanStream) {
      final device = BluetoothDevice(
        remoteId: DeviceIdentifier(s.address),
        localName: s.name,
        type: BluetoothDeviceType.unknown,
      );
      print(s.manufacturerData);
      final result = ScanResult(
        device: device,
        advertisementData: AdvertisementData(
          localName: s.name,
          txPowerLevel: null,
          // TODO: implementation missing
          connectable: !s.advType.contains('Non'),
          manufacturerData: {
            if (s.manufacturerData.length >= 2)
              s.manufacturerData[0]: s.manufacturerData.sublist(2),
          },
          serviceData: {},
          // TODO: implementation missing
          serviceUuids: s.serviceUuids.map((e) => e as String).toList(),
        ),
        rssi: int.parse(s.rssi),
        timeStamp: DateTime.now(),
      );
      list.add(result);
      yield list;
    }
  }

  static Stream<BluetoothAdapterState> get adapterState async* {
    await _initialize();
    await for (final s in Stream<BluetoothAdapterState>.empty()) {
      yield s;
    }
  }

  static Future<List<BluetoothDevice>> get connectedSystemDevices async {
    await _initialize();
    return [];
  }

  static Future<List<BluetoothDevice>> get bondedDevices async {
    await _initialize();
    return [];
  }

  static Stream<ScanResult> scan({
    ScanMode scanMode = ScanMode.lowLatency,
    List<Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) async* {
    await _initialize();
    // await for (final s in Stream<ScanResult>.empty()) {
    //   yield s;
    // }
  }

  static Future<void> startScan({
    ScanMode scanMode = ScanMode.lowLatency,
    List<Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) async {
    await _initialize();
    WinBle.startScanning();

    if (timeout != null) {
      await Future.delayed(
        timeout,
        () {
          stopScan();
        },
      );
    }
  }

  /// Stops a scan for Bluetooth Low Energy devices
  static Future<void> stopScan() async {
    await _initialize();
    WinBle.stopScanning();
  }

  /// Sets the internal FlutterBlue log level
  static void setLogLevel(LogLevel level, {color = true}) {
    return;
  }

  @Deprecated('Deprecated in Android SDK 33 with no replacement')
  static Future<void> turnOff({int timeout = 10}) async {
    await _initialize();
    await WinBle.updateBluetoothState(false);
  }

  static Future<bool> get isOn async {
    await _initialize();
    return await WinBle.bleState.first == BleState.On;
  }
}
