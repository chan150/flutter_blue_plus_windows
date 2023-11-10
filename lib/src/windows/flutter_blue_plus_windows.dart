part of 'windows.dart';

class FlutterBluePlusWindows {
  static bool _initialized = false;

  static BluetoothAdapterState _state = BluetoothAdapterState.unknown;

  // stream used for the isScanning public api
  static final _isScanning = _StreamController(initialValue: false);

  // we always keep track of these device variables
  static final _knownServices =
      <DeviceIdentifier, List<BluetoothServiceWindows>>{};
  static final Map<DeviceIdentifier, Map<String, List<int>>> _lastChrs = {};
  static final Map<DeviceIdentifier, Map<String, bool>> _isNotifying = {};

  static final Map<DeviceIdentifier, Map<String, List<BluetoothCharacteristic>>>
      _characteristicCache = {};

  // static final Map<DeviceIdentifier, List<BluetoothCharacteristicWindows>>
  //     _notifiedChrs = {};

  // stream used for the scanResults public api
  static final _scanResultsList =
      _StreamController(initialValue: <ScanResult>[]);

  // the subscription to the scan results stream
  static StreamSubscription<BleDevice?>? _scanSubscription;

  // timeout for scanning that can be cancelled by stopScan
  static Timer? _scanTimeout;

  static List<BluetoothDeviceWindows> get _devices =>
      _added.difference(_removed).toList();
  static final _removed = <BluetoothDeviceWindows>{};
  static final _added = <BluetoothDeviceWindows>{};

  /// newly defined
  static final _connectionStream =
      _StreamController(initialValue: <String, bool>{});

  static Future<void> _initialize() async {
    if (_initialized) return;
    await WinBle.initialize(
      serverPath: await WinServer.path,
      enableLog: false,
    );

    WinBle.connectionStream.listen(
      (event) {
        log('$event - event');
        if (event['device'] == null) return;
        if (event['connected'] == null) return;

        final map = _connectionStream.latestValue;
        map[event['device']] = event['connected'];

        log('$map - map');
        _connectionStream.add(map);

        if (!event['connected']) {
          final _device = _added
              .where((device) => device._address == event['device'])
              .firstOrNull;
          if (_device != null && !_removed.contains(_device)) {
            _removed.add(_device);
          }
          // _devices.removeWhere((device) => device._address == event['device']);
        }
      },
    );
    _initialized = true;
  }

  static Future<bool> get isSupported async {
    return true;
  }

  static Future<String> get adapterName async {
    return 'Windows';
  }

  static Stream<bool> get isScanning async* {
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

  // TODO: compare with original lib
  static Stream<List<ScanResult>> get scanResults => _scanResultsList.stream;

  static Stream<BluetoothAdapterState> get adapterState async* {
    await _initialize();
    yield _state;
    yield* WinBle.bleState.asBroadcastStream().map(
      (s) {
        _state = s.toAdapterState();
        return _state;
      },
    );
  }

  static List<BluetoothDevice> get connectedDevices {
    return _devices;
  }

  static Future<List<BluetoothDevice>> get bondedDevices async {
    return _devices;
  }

  /// Start a scan, and return a stream of results
  ///   - [timeout] calls stopScan after a specified duration
  ///   - [removeIfGone] if true, remove devices after they've stopped advertising for X duration
  ///   - [oneByOne] if true, we will stream every advertistment one by one, including duplicates.
  ///    If false, we deduplicate the advertisements, and return a list of devices.
  ///   - [androidUsesFineLocation] request ACCESS_FINE_LOCATION permission at runtime
  static Future<void> startScan({
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

    // Start timer *after* stream is being listened to, to make sure the
    // timeout does not fire before _buffer is set
    if (timeout != null) {
      _scanTimeout = Timer(timeout, stopScan);
    }

    /// remove connection by OS.
    /// The reason why we add this logic is
    /// to avoid uncontrollable devices and to make consistency.
    for (final device in _removed) {
      await WinBle.connect(device._address);
      await WinBle.disconnect(device._address);
    }
    _added.removeAll(_removed);
    _removed.clear();

    /// add WinBle scanning
    WinBle.startScanning();

    // check every 250ms for gone devices?
    late Stream<BleDevice?> outputStream;
    if (removeIfGone != null) {
      outputStream = _mergeStreams(
        [WinBle.scanStream, Stream.periodic(Duration(milliseconds: 250))],
      );
    } else {
      outputStream = WinBle.scanStream;
    }

    final output = <ScanResult>[];

    // listen & push to `scanResults` stream
    _scanSubscription = outputStream.listen(
      (BleDevice? winBleDevice) {
        if (winBleDevice == null) {
          // if null, this is just a periodic update for removing old results
          output.removeWhere((elm) =>
              DateTime.now().difference(elm.timeStamp) > removeIfGone!);

          // push to stream
          _scanResultsList.add(List.from(output));
        } else {
          final device = BluetoothDeviceWindows(
            platformName: winBleDevice.name,
            remoteId: DeviceIdentifier(winBleDevice.address.toUpperCase()),
            rssi: int.tryParse(winBleDevice.rssi) ?? -100,
          );
          final sr = ScanResult(
            device: device,
            advertisementData: AdvertisementData(
              advName: winBleDevice.name,
              txPowerLevel: winBleDevice.adStructures
                  ?.where((e) => e.type == 10)
                  .singleOrNull
                  ?.data
                  .firstOrNull,
              //TODO: Should verify
              connectable: !winBleDevice.advType.contains('Non'),
              manufacturerData: {
                if (winBleDevice.manufacturerData.length >= 2)
                  winBleDevice.manufacturerData[0]:
                      winBleDevice.manufacturerData.sublist(2),
              },
              //TODO: implementation missing
              serviceData: {},
              serviceUuids: winBleDevice.serviceUuids
                  .map(
                      (e) => Guid((e as String).replaceAll(RegExp(r'{|}'), '')))
                  .toList(),
            ),
            rssi: int.tryParse(winBleDevice.rssi) ?? -100,
            timeStamp: DateTime.now(),
          );

          // add result to output
          if (oneByOne) {
            output
              ..clear()
              ..add(sr);
          } else {
            output.addOrUpdate(sr);
          }

          // push to stream
          _scanResultsList.add(List.from(output));
        }
      },
    );
  }

  /// Stops a scan for Bluetooth Low Energy devices
  static Future<void> stopScan() async {
    await _initialize();
    WinBle.stopScanning();
    _scanSubscription?.cancel();
    _scanTimeout?.cancel();
    _isScanning.add(false);

    _scanResultsList.latestValue = [];
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
