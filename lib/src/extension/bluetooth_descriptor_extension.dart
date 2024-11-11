import 'package:flutter_blue_plus/flutter_blue_plus.dart';

extension BluetoothDescriptorExtension on BluetoothDescriptor {
  BmBluetoothDescriptor toProto() {
    return BmBluetoothDescriptor(
      remoteId: DeviceIdentifier(remoteId.str),
      serviceUuid: serviceUuid,
      characteristicUuid: characteristicUuid,
      descriptorUuid: descriptorUuid,
      // primaryServiceUuid: null, // TODO:  API changes
    );
  }
}
