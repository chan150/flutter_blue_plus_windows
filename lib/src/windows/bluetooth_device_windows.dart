part of 'windows.dart';

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
