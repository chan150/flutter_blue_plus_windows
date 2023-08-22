import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:win_ble/win_ble.dart';

class BluetoothDeviceWindows extends BluetoothDevice {
  BluetoothDeviceWindows({
    required super.remoteId,
    required super.localName,
    required super.type,
    required this.device,
  });

  final BleDevice device;

  @override
  List<BluetoothService>? get servicesList => [];
}
