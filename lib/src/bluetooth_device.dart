import 'package:flutter_blue_plus/flutter_blue_plus.dart' as BLE;

class BluetoothDevice {

  final BLE.DeviceIdentifier remoteId;
  final String localName;
  final BLE.BluetoothDeviceType type;

  BluetoothDevice({
    required this.remoteId,
    required this.localName,
    required this.type,
  });
}
