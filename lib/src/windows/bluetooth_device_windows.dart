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


}
