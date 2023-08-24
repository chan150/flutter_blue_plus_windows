// Bluetooth Device Page:
// https://github.com/boskokg/flutter_blue_plus/blob/master/lib/src/bluetooth_device.dart

part of 'windows.dart';

class BluetoothDeviceWindows extends BluetoothDevice {
  BluetoothDeviceWindows({
    required super.remoteId,
    required super.localName,
    required super.type,
    this.rssi,
  });

  final int? rssi;

  // used for 'servicesStream' public api
  final _services = StreamController<List<BluetoothServiceWindows>>.broadcast();

  // late final _connectionStream = WinBle.connectionStreamOf(remoteId.str);

  // used for 'isDiscoveringServices' public api
  final _isDiscoveringServices = CachedStreamController(initialValue: false);

  StreamSubscription<bool>? _subscription;
  
  String get _address => remoteId.str.toLowerCase();

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
        FlutterBluePlusWindows._knownServices[remoteId]!,
      );
    } else {
      return _services.stream;
    }
  }

  Future<void> connect({
    Duration timeout = const Duration(seconds: 35),
    bool autoConnect = false,
  }) async {
    // bool isFinished = false;
    // StreamSubscription _connectionStream =
    //     WinBle.connectionStreamOf(_address).listen(
    //   (event) {
    //     if (isFinished) return;
    //     if (!event) return;
    //     if (FlutterBluePlusWindows._connectedDevices
    //         .where((e) => e.remoteId == remoteId)
    //         .isNotEmpty) return;
    //     FlutterBluePlusWindows._connectedDevices.add(this);
    //     isFinished = true;
    //   },
    // );
    // Timer(timeout, _connectionStream.cancel);

    try {
      await WinBle.connect(_address);
      final existed = FlutterBluePlusWindows._connectedDevices
          .where((e) => e.remoteId == remoteId)
          .isNotEmpty;
      if (!existed) {
        FlutterBluePlusWindows._connectedDevices.add(this);
        FlutterBluePlusWindows._connectedBehaviors[this] = BehaviorSubject();

        _subscription =
            WinBle.connectionStreamOf(_address).listen(
          (event) {
            if (!event) {
              _subscription?.cancel();
            }
          },
        );
      }
    } catch (e) {
      print(e);
    } finally {}
  }

  Future<void> disconnect({int timeout = 35}) async {
    // bool isFinished = false;
    // StreamSubscription connectionStream =
    //     WinBle.connectionStreamOf(_address).listen(
    //   (event) {
    //     if (isFinished) return;
    //     // if (event) return;
    //     // if (FlutterBluePlusWindows._connectedDevices
    //     //     .where((e) => e.remoteId == remoteId)
    //     //     .isEmpty) return;
    //     isFinished = true;
    //   },
    // );
    // final timer = Timer(Duration(seconds: timeout), connectionStream.cancel);

    try {
      await WinBle.disconnect(_address);
      FlutterBluePlusWindows._connectedDevices.remove(this);
    } catch (e) {
      print(e);
    } finally {
      // connectionStream.cancel();
      // timer.cancel();
    }

    print('-------------------------------');
  }

  Future<List<BluetoothService>> discoverServices({int timeout = 15}) async {
    List<BluetoothServiceWindows> result = [];

    try {
      _isDiscoveringServices.add(true);

      final response =
          await WinBle.discoverServices(_address);

      result = response.map(
        (p) {
          return BluetoothServiceWindows(
            remoteId: remoteId,
            serviceUuid: Guid(p),
            // TODO: implementation missing
            isPrimary: true,
            // TODO: implementation missing
            characteristics: [],
            // TODO: implementation missing
            includedServices: [],
          );
        },
      ).toList();

      FlutterBluePlusWindows._knownServices[remoteId] = result;

      _services.add(result);
    } finally {
      _isDiscoveringServices.add(false);
    }
    return result;
  }

  DisconnectReason? get disconnectReason {
    return null;
  }

  Stream<BluetoothConnectionState> get connectionState async* {
    BluetoothConnectionState initialValue =
        BluetoothConnectionState.disconnected;

    // WinBle.connectionStream
  }

  Stream<int> get mtu {
    // TODO: do implementation
    throw Exception('Missing implementation');
  }

  Future<int> readRssi({int timeout = 15}) async {
    return rssi ?? -100;
  }

  Future<int> requestMtu(int desiredMtu, {int timeout = 15}) async {
    return await WinBle.getMaxMtuSize(_address);
  }

  Future<void> requestConnectionPriority({
    required ConnectionPriority connectionPriorityRequest,
  }) async {
    // TODO: implementation missing
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
    // TODO: implementation missing
  }

  Future<void> createBond({int timeout = 90}) async {
    // TODO: implementation missing
  }

  Future<void> removeBond({int timeout = 30}) async {
    // TODO: implementation missing
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
