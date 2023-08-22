part of 'windows.dart';

class FlutterBluePlusWindows {
  static final _scanResultsList =
      CachedStreamController(initialValue: <ScanResult>[]);
  static final _scanningController =
      CachedStreamController<bool>(initialValue: false);

  // static final _scanningStream = _scanningController.stream;

  static bool get _isScanningNow => _scanningController.latestValue;
  static bool _initialized = false;

  // timeout for scanning that can be cancelled by stopScan
  static Timer? _scanTimeout;

  static final _connected = <BluetoothDeviceWindows>[];

  static final _bonded = <BluetoothDeviceWindows>[];

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
    await for (final s in _scanningController.stream) {
      yield s;
    }
  }

  static bool get isScanningNow {
    return _isScanningNow;
  }

  static Future<void> turnOn({int timeout = 10}) async {
    await _initialize();
    await WinBle.updateBluetoothState(true);
  }

  static Stream<List<ScanResult>> get scanResults => _scanResultsList.stream;

  static Stream<BluetoothAdapterState> get adapterState async* {
    await _initialize();
    await for (final s in WinBle.bleState.asBroadcastStream()) {
      yield s.toAdapterState();
    }
  }

  static Future<List<BluetoothDevice>> get connectedSystemDevices async {
    await _initialize();
    return _connected;
  }

  static Future<List<BluetoothDevice>> get bondedDevices async {
    await _initialize();
    return _bonded;
  }

  static Stream<ScanResult> scan({
    ScanMode scanMode = ScanMode.lowLatency, // TODO: implementation missing
    List<Guid> withServices = const [], // TODO: implementation missing
    List<String> macAddresses = const [], // TODO: implementation missing
    Duration? timeout,
    bool allowDuplicates = false, // TODO: implementation missing
    bool androidUsesFineLocation = false, // nothing to implement
  }) async* {
    await _initialize();

    WinBle.startScanning();
    _scanningController.add(true);

    final list = <ScanResult>[];
    _scanResultsList.add(list);

    if (timeout != null) {
      _scanTimeout = Timer(timeout, stopScan);
    }

    await for (final s in WinBle.scanStream) {
      final device = BluetoothDeviceWindows(
        remoteId: DeviceIdentifier(s.address),
        localName: s.name,
        type: s.adStructures
                ?.where((e) => e.type == 1)
                .singleOrNull
                .toDeviceType() ??
            BluetoothDeviceType.unknown,
        device: s,
      );
      final item = ScanResult(
        device: device,
        advertisementData: AdvertisementData(
          localName: s.name,
          txPowerLevel: s.adStructures
              ?.where((e) => e.type == 10)
              .singleOrNull
              ?.data
              .firstOrNull,
          // TODO: Should verify
          connectable: !s.advType.contains('Non'),
          manufacturerData: {
            if (s.manufacturerData.length >= 2)
              s.manufacturerData[0]: s.manufacturerData.sublist(2),
          },
          // TODO: implementation missing
          serviceData: {},
          serviceUuids: s.serviceUuids.map((e) => e as String).toList(),
        ),
        rssi: int.parse(s.rssi),
        timeStamp: DateTime.now(),
      );

      List<ScanResult> list = addOrUpdate(_scanResultsList.value, item);

      // update list
      _scanResultsList.add(list);

      yield item;
    }
    _scanTimeout?.cancel();
    _scanningController.add(false);
  }

  static Future startScan({
    ScanMode scanMode = ScanMode.lowLatency,
    List<Guid> withServices = const [],
    List<String> macAddresses = const [],
    Duration? timeout,
    bool allowDuplicates = false,
    bool androidUsesFineLocation = false,
  }) async {
    await _initialize();

    await scan(
      scanMode: scanMode,
      withServices: withServices,
      macAddresses: macAddresses,
      timeout: timeout,
      allowDuplicates: allowDuplicates,
      androidUsesFineLocation: androidUsesFineLocation,
    ).drain();

    return _scanResultsList.value;
  }

  /// Stops a scan for Bluetooth Low Energy devices
  static Future<void> stopScan() async {
    await _initialize();
    WinBle.stopScanning();
    _scanTimeout?.cancel();
    _scanningController.add(false);
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

  // TODO: need to test
  static Future<bool> get isOn async {
    await _initialize();
    return await WinBle.bleState.asBroadcastStream().first == BleState.On;
  }
}
