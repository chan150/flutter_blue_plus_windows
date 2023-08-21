import 'package:flutter_blue_plus/flutter_blue_plus.dart' as BLE;

typedef BluetoothDevice = BLE.BluetoothDevice;

extension BluetoothDeviceWindows on BluetoothDevice {
  Future<void> connect({
    Duration timeout = const Duration(seconds: 35),
    bool autoConnect = false,
    bool tt = false,
  }) async {

  }

  t(){}
}
