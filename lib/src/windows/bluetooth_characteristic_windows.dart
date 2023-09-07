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

  // TODO: need to verify
  Stream<List<int>> get onValueReceived async* {
    await WinBle.subscribeToCharacteristic(
      address: _address,
      serviceId: serviceUuid.toString(),
      characteristicId: characteristicUuid.toString(),
    );
    final stream = WinBle.characteristicValueStreamOf(
      address: remoteId.str.toLowerCase(),
      serviceId: serviceUuid.toString(),
      characteristicId: characteristicUuid.toString(),
    );
    await for (final event in stream) {
      yield event.value as List<int>;
    }
  }

// FlutterBluePlus._methodStream.stream
//     .where((m) => m.method == "OnCharacteristicReceived")
//     .map((m) => m.arguments)
//     .map((args) => BmOnCharacteristicReceived.fromMap(args))
//     .where((p) => p.remoteId == remoteId.toString())
//     .where((p) => p.serviceUuid == serviceUuid)
//     .where((p) => p.characteristicUuid == characteristicUuid)
//     .where((p) => p.success == true)
//     .map(
//   (c) {
//     lastValue = c.value; // Update cache of lastValue
//     return c.value;
//   },
// );

  // TODO: implementation is required
  bool get isNotifying => false;

  Future<List<int>> read({int timeout = 15}) async {
    final value = await WinBle.read(
      address: remoteId.str.toLowerCase(),
      serviceId: serviceUuid.toString(),
      characteristicId: characteristicUuid.toString(),
    );

    return value;
  }

  Future<void> write(
    List<int> value, {
    bool allowLongWrite = false,
    bool withoutResponse = false,
    int timeout = 15,
  }) async {
    await WinBle.write(
      address: _address,
      service: serviceUuid.toString(),
      characteristic: characteristicUuid.toString(),
      data: Uint8List.fromList(value),
      writeWithResponse: propertiesWinBle.writeWithoutResponse ?? false,
    );
  }

  // TODO: need to verify
  Future<bool> setNotifyValue(
    bool notify, {
    int timeout = 15,
  }) async {
    // if(notify){
    //   await WinBle.subscribeToCharacteristic(
    //     address: _address,
    //     serviceId: serviceUuid.toString(),
    //     characteristicId: characteristicUuid.toString(),
    //   );
    // } else {
    //   await WinBle.unSubscribeFromCharacteristic(
    //     address: _address,
    //     serviceId: serviceUuid.toString(),
    //     characteristicId: characteristicUuid.toString(),
    //   );
    // }
    return true;
  }
}
