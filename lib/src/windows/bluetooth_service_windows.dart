import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

class BluetoothServiceWindows extends BluetoothService {
  final DeviceIdentifier remoteId;
  final Guid serviceUuid;
  final bool isPrimary;
  final List<BluetoothCharacteristic> characteristics;
  final List<BluetoothService> includedServices;

  BluetoothServiceWindows({
    required this.remoteId,
    required this.serviceUuid,
    required this.isPrimary,
    required this.characteristics,
    required this.includedServices,
  }) : super.fromProto(
          BmBluetoothService(
            remoteId: remoteId.str,
            serviceUuid: serviceUuid,
            characteristics: [for (final c in characteristics) c.toProto()],
            isPrimary: isPrimary,
            includedServices: [for (final s in includedServices) s.toProto()],
          ),
        );
}
