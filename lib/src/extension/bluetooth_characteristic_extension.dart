import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

extension BluetoothCharacteristicExtension on BluetoothCharacteristic {
  BmBluetoothCharacteristic toProto() {
    return BmBluetoothCharacteristic(
      remoteId: remoteId.str,
      serviceUuid: serviceUuid,
      secondaryServiceUuid: secondaryServiceUuid,
      characteristicUuid: characteristicUuid,
      descriptors: [for (final d in descriptors) d.toProto()],
      properties: properties.toProto(),
      value: lastValue,
    );
  }
}
