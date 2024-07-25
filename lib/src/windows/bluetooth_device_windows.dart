// Bluetooth Device Page:
// https://github.com/boskokg/flutter_blue_plus/blob/master/lib/src/bluetooth_device.dart

part of 'windows.dart';

class BluetoothDeviceWindows extends BluetoothDevice {
  BluetoothDeviceWindows({
    required this.platformName,
    required super.remoteId,
    this.rssi,
  });

  final int? rssi;

  final String platformName;

  // used for 'servicesStream' public api
  final _services = StreamController<List<BluetoothServiceWindows>>.broadcast();

  // late final _connectionStream = WinBle.connectionStreamOf(remoteId.str);

  // used for 'isDiscoveringServices' public api
  final _isDiscoveringServices = _StreamController(initialValue: false);

  // StreamSubscription<bool>? _subscription;

  String get _address => remoteId.str.toLowerCase();

  // stream return whether or not we are currently discovering services
  @Deprecated(
      "planed for removal (Jan 2024). It can be easily implemented yourself") // deprecated on Aug 2023
  Stream<bool> get isDiscoveringServices => _isDiscoveringServices.stream;

  // // Get services
  // //  - returns null if discoverServices() has not been called
  // //  - this is cleared on disconnection. You must call discoverServices() again
  // List<BluetoothService>? get servicesList =>
  //     FlutterBluePlusWindows._knownServices[remoteId];

  /// Get services
  ///  - returns empty if discoverServices() has not been called
  ///    or if your device does not have any services (rare)
  List<BluetoothServiceWindows> get servicesList =>
      FlutterBluePlusWindows._knownServices[remoteId] ?? [];

  /// Stream of bluetooth services offered by the remote device
  ///   - this stream is only updated when you call discoverServices()
  @Deprecated(
      "planed for removal (Jan 2024). It can be easily implemented yourself") // deprecated on Aug 2023
  Stream<List<BluetoothService>> get servicesStream {
    if (FlutterBluePlusWindows._knownServices[remoteId] != null) {
      return _services.stream.newStreamWithInitialValue(
        FlutterBluePlusWindows._knownServices[remoteId]!,
      );
    } else {
      return _services.stream;
    }
  }

  Future<void> connect({
    Duration? timeout =
        const Duration(seconds: 35), // TODO: implementation missing
    bool autoConnect = false, // TODO: implementation missing
    int? mtu = 512, // TODO: implementation missing
  }) async {
    try {
      await WinBle.connect(_address);
    } catch (e) {
      print(e);
    } finally {
      FlutterBluePlusWindows._deviceSet.add(this);
      // FlutterBluePlusWindows._removed.remove(this);
    }
  }

  Future<void> disconnect({
    int androidDelay = 2000, // TODO: implementation missing
    int timeout = 35, // TODO: implementation missing
    bool queue = true, // TODO: implementation missing
  }) async {
    try {
      // TODO: bug in WinBle; devices once paired are never connected unless the bonding is removed
      // await WinBle.unPair(_address);
      await WinBle.disconnect(_address);
    } catch (e) {
      print(e);
    } finally {
      FlutterBluePlusWindows._deviceSet.remove(this);
      // FlutterBluePlusWindows._removed.add(this);

      FlutterBluePlusWindows._lastChrs[remoteId]?.clear();
      FlutterBluePlusWindows._isNotifying[remoteId]?.clear();
    }
  }

  Future<List<BluetoothService>> discoverServices({
    bool subscribeToServicesChanged = true, // TODO: implementation missing
    int timeout = 15, // TODO: implementation missing
  }) async {
    List<BluetoothServiceWindows> result =
        List.from(FlutterBluePlusWindows._knownServices[remoteId] ?? []);

    try {
      _isDiscoveringServices.add(true);

      final response = await WinBle.discoverServices(_address);
      FlutterBluePlusWindows._characteristicCache[remoteId] ??=
          <String, List<BluetoothCharacteristic>>{};

      for (final serviceId in response) {
        final characteristic = await WinBle.discoverCharacteristics(
          address: _address,
          serviceId: serviceId,
        );
        FlutterBluePlusWindows._characteristicCache[remoteId]?[serviceId] ??= [
          ...characteristic.map(
            (e) => BluetoothCharacteristicWindows(
              remoteId: remoteId,
              serviceUuid: Guid(serviceId),
              characteristicUuid: Guid(e.uuid),
              descriptors: [],
              // TODO: implementation missing
              propertiesWinBle: e.properties,
            ),
          ),
        ];
      }

      result = response
          .map(
            (p) => BluetoothServiceWindows(
              remoteId: remoteId,
              serviceUuid: Guid(p),
              // TODO: implementation missing
              isPrimary: true,
              // TODO: implementation missing
              characteristics:
                  FlutterBluePlusWindows._characteristicCache[remoteId]![p]!,
              // TODO: implementation missing
              includedServices: [],
            ),
          )
          .toList();

      FlutterBluePlusWindows._knownServices[remoteId] = result;

      _services.add(result);
    } finally {
      _isDiscoveringServices.add(false);
    }
    return result;
  }

  DisconnectReason? get disconnectReason {
    // TODO: nothing to do
    return null;
  }

  Stream<BluetoothConnectionState> get connectionState async* {
    await FlutterBluePlusWindows._initialize();

    final map = FlutterBluePlusWindows._connectionStream.latestValue;

    log('Connection State is started');

    if (map[_address] != null) {
      yield map[_address]!.isConnected;
    }

    await for (final event in WinBle.connectionStreamOf(_address)) {
      yield event.isConnected;
    }

    log('Connection State is closed');
  }

  Stream<int> get mtu async* {
    yield rssi ?? -100;
  }

  Future<int> readRssi({int timeout = 15}) async {
    return rssi ?? -100;
  }

  Future<int> requestMtu(
    int desiredMtu, {
    double predelay = 0.35,
    int timeout = 15,
  }) async {
    return await WinBle.getMaxMtuSize(_address);
  }

  Future<void> requestConnectionPriority({
    required ConnectionPriority connectionPriorityRequest,
  }) async {
    // TODO: nothing to do
    return;
  }

  /// Set the preferred connection (Android Only)
  ///   - [txPhy] bitwise OR of all allowed phys for Tx, e.g. (Phy.le2m.mask | Phy.leCoded.mask)
  ///   - [txPhy] bitwise OR of all allowed phys for Rx, e.g. (Phy.le2m.mask | Phy.leCoded.mask)
  ///   - [option] preferred coding to use when transmitting on Phy.leCoded
  /// Please note that this is just a recommendation given to the system.
  Future<void> setPreferredPhy({
    required int txPhy,
    required int rxPhy,
    required PhyCoding option,
  }) async {
    // TODO: implementation missing
  }

  Future<void> createBond({
    int timeout = 90, // TODO: implementation missing
  }) async {
    try {
      await WinBle.pair(_address);
    } catch (e) {
      log(e.toString());
    }
  }

  Future<void> removeBond({
    int timeout = 30, // TODO: implementation missing
  }) async {
    try {
      await WinBle.unPair(_address);
    } catch (e) {
      log(e.toString());
    }
  }

  Future<void> clearGattCache() async {
    // TODO: implementation missing
  }

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      (other is BluetoothDevice &&
          runtimeType == other.runtimeType &&
          remoteId == other.remoteId);

  @override
  int get hashCode => remoteId.hashCode;

  @override
  String toString() {
    return 'BluetoothDevice{'
        'remoteId: $remoteId, '
        'platformName: $platformName, '
        'services: ${FlutterBluePlusWindows._knownServices[remoteId]}'
        '}';
  }

  @Deprecated('Use createBond() instead')
  Future<void> pair() async => await createBond();

  @Deprecated('Use remoteId instead')
  DeviceIdentifier get id => remoteId;

  @Deprecated('Use localName instead')
  String get name => localName;

  @Deprecated('Use connectionState instead')
  Stream<BluetoothConnectionState> get state => connectionState;

  @Deprecated('Use servicesStream instead')
  Stream<List<BluetoothService>> get services => servicesStream;
}
