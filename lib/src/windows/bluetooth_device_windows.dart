// Bluetooth Device Page:
// https://github.com/boskokg/flutter_blue_plus/blob/master/lib/src/bluetooth_device.dart

part of 'windows.dart';

class BluetoothDeviceWindows extends BluetoothDevice {
  BluetoothDeviceWindows({
    required super.remoteId,
    required super.localName,
    required super.type,
    required this.device,
  });

  final BleDevice device;

  // used for 'servicesStream' public api
  final _services = StreamController<List<BluetoothServiceWindows>>.broadcast();

  late final _connectionStream = WinBle.connectionStreamOf(remoteId.str);

  // used for 'isDiscoveringServices' public api
  final _isDiscoveringServices = CachedStreamController(initialValue: false);

  // stream return whether or not we are currently discovering services
  @Deprecated(
      "planed for removal (Jan 2024). It can be easily implemented yourself") // deprecated on Aug 2023
  Stream<bool> get isDiscoveringServices => _isDiscoveringServices.stream;

  // Get services
  //  - returns null if discoverServices() has not been called
  //  - this is cleared on disconnection. You must call discoverServices() again
  List<BluetoothService>? get servicesList =>
      FlutterBluePlusWindows._knownServices[remoteId];

  /// Stream of bluetooth services offered by the remote device
  ///   - this stream is only updated when you call discoverServices()
  @Deprecated(
      "planed for removal (Jan 2024). It can be easily implemented yourself") // deprecated on Aug 2023
  Stream<List<BluetoothService>> get servicesStream {
    if (FlutterBluePlusWindows._knownServices[remoteId] != null) {
      return _services.stream.newStreamWithInitialValue(
          FlutterBluePlusWindows._knownServices[remoteId]!);
    } else {
      return _services.stream;
    }
  }

  Future<void> connect({
    Duration timeout = const Duration(seconds: 35),
    bool autoConnect = false,
  }) async {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<void> disconnect({int timeout = 35}) async {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<List<BluetoothService>> discoverServices({int timeout = 15}) async {
    throw Exception('Missing implementation');
    // TODO: do implementation
    return [];
  }

  DisconnectReason? get disconnectReason {
    throw Exception('Missing implementation');
    // TODO: do implementation
  }

  Stream<BluetoothConnectionState> get connectionState {
    throw Exception('Missing implementation');
    // TODO: do implementation
  }

  Stream<int> get mtu {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<int> readRssi({int timeout = 15}) async {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<int> requestMtu(int desiredMtu, {int timeout = 15}) async {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<void> requestConnectionPriority({
    required ConnectionPriority connectionPriorityRequest,
  }) async {
    // TODO: do implementation
    throw Exception('Missing implementation');
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
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<void> createBond({int timeout = 90}) async {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<void> removeBond({int timeout = 30}) async {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<void> clearGattCache() async {
    // TODO: do implementation
    throw Exception('Missing implementation');
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
        'localName: $localName, '
        'type: $type, '
        'isDiscoveringServices: ${_isDiscoveringServices.value}, '
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
