import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

extension BluetoothServiceExtension on BluetoothService {
  BmBluetoothService toProto() {
    return BmBluetoothService(
      serviceUuid: serviceUuid,
      remoteId: DeviceIdentifier(remoteId.str),
      // isPrimary: isPrimary,
      characteristics: [for (final c in characteristics) c.toProto()],
      // includedServices: [for (final s in includedServices) s.toProto()],
      primaryServiceUuid: null, // TODO:  API changes
    );
  }
}
