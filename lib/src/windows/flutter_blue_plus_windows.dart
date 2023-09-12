part of 'windows.dart';

class FlutterBluePlusWindows extends FlutterBluePlus {
  static final _knownServices =
      <DeviceIdentifier, List<BluetoothServiceWindows>>{};

  // stream used for the isScanning public api
  static final _isScanning = _StreamController<bool>(initialValue: false);

  static final _scanResultsList =
      _StreamController(initialValue: <ScanResult>[]);

  static bool _initialized = false;

  // timeout for scanning that can be cancelled by stopScan
  static Timer? _scanTimeout;

  static final _devices = <BluetoothDeviceWindows>[];
  static final _connectedBehaviors =
      <BluetoothDeviceWindows, BehaviorSubject<bool>>{};

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
    await for (final s in _isScanning.stream) {
      yield s;
    }
  }

  static bool get isScanningNow {
    return _isScanning.latestValue;
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
    return _devices
        .where((device) => _connectedBehaviors[device]?.valueOrNull ?? false)
        .toList();
  }

  static Future<List<BluetoothDevice>> get bondedDevices async {
    await _initialize();
    return _devices;
  }

  @Deprecated('removed. use startScan with the oneByOne option instead')
  static Stream<ScanResult> scan({
    ScanMode scanMode = ScanMode.lowLatency, // TODO: implementation missing
    List<Guid> withServices = const [], // TODO: implementation missing
    List<String> macAddresses = const [], // TODO: implementation missing
    Duration? timeout,
    bool allowDuplicates = false, // TODO: implementation missing
    bool androidUsesFineLocation = false, // nothing to implement
  }) =>
      throw Exception;

  // async* {
  //   await _initialize();
  //
  //   WinBle.startScanning();
  //   _scanningController.add(true);
  //
  //   final list = <ScanResult>[];
  //   _scanResultsList.add(list);
  //
  //   if (timeout != null) {
  //     _scanTimeout = Timer(timeout, stopScan);
  //   }
  //
  //   await for (final winBleDevice in WinBle.scanStream) {
  //     final device = BluetoothDeviceWindows(
  //         remoteId: DeviceIdentifier(winBleDevice.address.toUpperCase()),
  //         localName: winBleDevice.name,
  //         type: winBleDevice.adStructures
  //                 ?.where((e) => e.type == 1)
  //                 .singleOrNull
  //                 .toDeviceType() ??
  //             BluetoothDeviceType.unknown,
  //         rssi: int.tryParse(winBleDevice.rssi) ?? -100);
  //     final item = ScanResult(
  //       device: device,
  //       advertisementData: AdvertisementData(
  //         localName: winBleDevice.name,
  //         txPowerLevel: winBleDevice.adStructures
  //             ?.where((e) => e.type == 10)
  //             .singleOrNull
  //             ?.data
  //             .firstOrNull,
  //         // TODO: Should verify
  //         connectable: !winBleDevice.advType.contains('Non'),
  //         manufacturerData: {
  //           if (winBleDevice.manufacturerData.length >= 2)
  //             winBleDevice.manufacturerData[0]:
  //                 winBleDevice.manufacturerData.sublist(2),
  //         },
  //         // TODO: implementation missing
  //         serviceData: {},
  //         serviceUuids:
  //             winBleDevice.serviceUuids.map((e) => e as String).toList(),
  //       ),
  //       rssi: int.tryParse(winBleDevice.rssi) ?? -100,
  //       timeStamp: DateTime.now(),
  //     );
  //
  //     List<ScanResult> list = addOrUpdate(_scanResultsList.value, item);
  //
  //     // update list
  //     _scanResultsList.add(list);
  //
  //     yield item;
  //   }
  //   _scanTimeout?.cancel();
  //   _scanningController.add(false);
  // }

  /// Start a scan, and return a stream of results
  ///   - [timeout] calls stopScan after a specified duration
  ///   - [removeIfGone] if true, remove devices after they've stopped advertising for X duration
  ///   - [oneByOne] if true, we will stream every advertistment one by one, including duplicates.
  ///    If false, we deduplicate the advertisements, and return a list of devices.
  ///   - [androidUsesFineLocation] request ACCESS_FINE_LOCATION permission at runtime
  static Future startScan({
    List<Guid> withServices = const [],
    Duration? timeout,
    Duration? removeIfGone,
    bool oneByOne = false,
    bool androidUsesFineLocation = false,
  }) async {
    await _initialize();

    // stop existing scan
    if (_isScanning.latestValue == true) {
      await stopScan();
    }

    // push to stream
    _isScanning.add(true);

    /// add WinBle scanning
    WinBle.startScanning();

    /// Flutter Blue Plus 1.15.X
    // var settings = BmScanSettings(
    //             serviceUuids: withServices,
    //             macAddresses: [],
    //             allowDuplicates: true,
    //             androidScanMode: ScanMode.lowLatency.value,
    //             androidUsesFineLocation: androidUsesFineLocation);
    //
    //         Stream<BmScanResponse> responseStream = FlutterBluePlus._methodStream.stream
    //             .where((m) => m.method == "OnScanResponse")
    //             .map((m) => m.arguments)
    //             .map((args) => BmScanResponse.fromMap(args));
    //
    //         // Start listening now, before invokeMethod, so we do not miss any results
    //         _BufferStream<BmScanResponse> _scanBuffer = _BufferStream.listen(responseStream);

    // Start timer *after* stream is being listened to, to make sure the
    // timeout does not fire before _buffer is set
    if (timeout != null) {
      _scanTimeout = Timer(timeout, stopScan);
    }

    /// Flutter Blue Plus 1.15.X
    // // invoke platform method
    // await _invokeMethod('startScan', settings.toMap());
    //
    // check every 250ms for gone devices?
    // late Stream<BmScanResponse?> outputStream;
    // if (removeIfGone != null) {
    //   outputStream = _mergeStreams(
    //       [_scanBuffer.stream, Stream.periodic(Duration(milliseconds: 250))]);
    // } else {
    //   outputStream = _scanBuffer.stream;
    // }

    final output = <ScanResult>[];
    // _scanResultsList.add(List.from(output));

    return _scanResultsList.value;
  }

  /// Stops a scan for Bluetooth Low Energy devices
  static Future<void> stopScan() async {
    await _initialize();
    WinBle.stopScanning();
    _scanTimeout?.cancel();
    _isScanning.add(false);
  }

  /// Sets the internal FlutterBlue log level
  static void setLogLevel(LogLevel level, {color = true}) {
    // Nothing to implement
    return;
  }

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
