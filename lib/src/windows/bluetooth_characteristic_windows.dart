part of 'windows.dart';

class BluetoothCharacteristicWindows extends BluetoothCharacteristic {
  final DeviceIdentifier remoteId;
  final Guid serviceUuid;
  final Guid? secondaryServiceUuid;
  final Guid characteristicUuid;
  final List<BluetoothDescriptor> descriptors;

  final Properties propertiesWinBle;

  BluetoothCharacteristicWindows({
    required this.remoteId,
    required this.serviceUuid,
    required this.characteristicUuid,
    required this.descriptors,
    required this.propertiesWinBle,
    this.secondaryServiceUuid,
  }) : super.fromProto(
          BmBluetoothCharacteristic(
            remoteId: remoteId.str,
            serviceUuid: serviceUuid,
            secondaryServiceUuid: secondaryServiceUuid,
            characteristicUuid: characteristicUuid,
            descriptors: [
              for (final descriptor in descriptors)
                BmBluetoothDescriptor(
                  remoteId: descriptor.remoteId.str,
                  serviceUuid: descriptor.serviceUuid,
                  characteristicUuid: descriptor.characteristicUuid,
                  descriptorUuid: descriptor.uuid,
                ),
            ],
            properties: BmCharacteristicProperties(
              broadcast: propertiesWinBle.broadcast ?? false,
              read: propertiesWinBle.read ?? false,
              writeWithoutResponse:
                  propertiesWinBle.writeWithoutResponse ?? false,
              write: propertiesWinBle.write ?? false,
              notify: propertiesWinBle.notify ?? false,
              indicate: propertiesWinBle.indicate ?? false,
              authenticatedSignedWrites:
                  propertiesWinBle.authenticatedSignedWrites ?? false,
              // TODO: implementation missing
              extendedProperties: false,
              // TODO: implementation missing
              notifyEncryptionRequired: false,
              // TODO: implementation missing
              indicateEncryptionRequired: false,
            ),
          ),
        );

  String get _address => remoteId.str.toLowerCase();

  String get _key => "$serviceUuid:$characteristicUuid";

  StreamSubscription? _subscription;

  late _StreamController _streamController;

  /// this variable is updated:
  ///   - anytime `read()` is called
  ///   - anytime `write()` is called
  ///   - anytime a notification arrives (if subscribed)
  List<int> get lastValue =>
      FlutterBluePlus._lastChrs[remoteId]?[_key] ?? [];

  /// this stream emits values:
  ///   - anytime `read()` is called (TODO: does not work)
  ///   - anytime `write()` is called (TODO: does not work)
  ///   - anytime a notification arrives (if subscribed)
  ///   - and when first listened to, it re-emits the last value for convenience
  // TODO: need to verify
  Stream<List<int>> get lastValueStream => WinBle.characteristicValueStreamOf(
        address: _address,
        serviceId: serviceUuid.uuid128,
        characteristicId: characteristicUuid.uuid128,
      )
          .map((p) => <int>[...p])
          .newStreamWithInitialValue(lastValue)
          .asBroadcastStream();

  /// this stream emits values:
  ///   - anytime `read()` is called (TODO: does not work)
  ///   - anytime a notification arrives (if subscribed)
  // TODO: need to verify
  Stream<List<int>> get onValueReceived => WinBle.characteristicValueStreamOf(
        address: _address,
        serviceId: serviceUuid.uuid128,
        characteristicId: characteristicUuid.uuid128,
      ).map((p) => <int>[...p]).asBroadcastStream();

  // TODO: implementation is required
  bool get isNotifying =>
      FlutterBluePlus._isNotifying[remoteId]?[_key] ?? false;

  Future<List<int>> read({int timeout = 15}) async {
    final value = await WinBle.read(
      address: _address,
      serviceId: serviceUuid.uuid128,
      characteristicId: characteristicUuid.uuid128,
    );

    return value;
  }

  Future<void> write(List<int> value,
      {bool allowLongWrite = false,
      bool withoutResponse = false,
      int timeout = 15}) async {
    await WinBle.write(
      address: _address,
      service: serviceUuid.uuid128,
      characteristic: characteristicUuid.uuid128,
      data: Uint8List.fromList(value),
      writeWithResponse: propertiesWinBle.writeWithoutResponse ?? false,
    );
  }

  // TODO: need to verify
  Future<bool> setNotifyValue(
    bool notify, {
    int timeout = 15, // TODO: missing implementation
    bool forceIndications = false,  // TODO: missing implementation
  }) async {
    /// unSubscribeFromCharacteristic
    try {
      await WinBle.unSubscribeFromCharacteristic(
        address: _address,
        serviceId: serviceUuid.uuid128,
        characteristicId: characteristicUuid.uuid128,
      );
    } catch (e) {
      log('WinBle.unSubscribeFromCharacteristic was performed '
          'before setNotifyValue()');
    }

    /// set notify
    try {
      if (notify) {
        await WinBle.subscribeToCharacteristic(
          address: _address,
          serviceId: serviceUuid.uuid128,
          characteristicId: characteristicUuid.uuid128,
        );
      }
      FlutterBluePlus._isNotifying[remoteId]?[_key] = notify;
    } catch (e) {
      log(e.toString());
    }
    return true;
  }
}
