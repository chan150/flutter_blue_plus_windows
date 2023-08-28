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
                  value: descriptor.lastValue,
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
              extendedProperties: false,
              // TODO: implementation missing
              notifyEncryptionRequired: false,
              // TODO: implementation missing
              indicateEncryptionRequired: false, // TODO: implementation missing
            ),
            value: [],
          ),
        );

  Stream<List<int>> get onValueReceived => WinBle.characteristicValueStreamOf(
        address: remoteId.str.toLowerCase(),
        serviceId: serviceUuid.toString(),
        characteristicId: characteristicUuid.toString(),
      ).map(
        (c) {
          lastValue = c.value; // Update cache of lastValue
          return c.value;
        },
      );

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
    lastValue = value;

    return value;
  }

  Future<void> write(List<int> value,
      {bool withoutResponse = false, int timeout = 15}) async {
    WinBle.write(
      address: remoteId.str.toLowerCase(),
      service: serviceUuid.toString(),
      characteristic: characteristicUuid.toString(),
      data: Uint8List.fromList(value),
      writeWithResponse: propertiesWinBle.writeWithoutResponse ?? false,
    );
  }

  // TODO: implementation is required
  Future<bool> setNotifyValue(bool notify, {int timeout = 15}) async {
    return true;
  }
}
