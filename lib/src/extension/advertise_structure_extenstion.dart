import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:win_ble/win_ble.dart';

extension AdStructureExtension on AdStructure? {
  // TODO: should verify
  BluetoothDeviceType toDeviceType(){
    if(this == null) return BluetoothDeviceType.unknown;
    final data = this?.data.singleOrNull;
    if(data == null) return BluetoothDeviceType.unknown;
    if(data == 8) return BluetoothDeviceType.classic;
    if(data == 9) return BluetoothDeviceType.dual;
    if(data == 10) return BluetoothDeviceType.dual;
    return BluetoothDeviceType.le;
  }
}