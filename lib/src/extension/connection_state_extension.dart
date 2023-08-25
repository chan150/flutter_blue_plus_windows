// import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';
//
// extension BluetoothAdapterStateExtension on BleState {
//   BluetoothConnectionState toAdapterState() {
//     switch(this){
//       case BleState.On:
//         return BluetoothConnectionState.connected;
//       case BleState.Off:
//         return BluetoothConnectionState.disconnected;
//       case BleState.Unknown:
//         return BluetoothAdapterState.unknown;
//       case BleState.Disabled:
//         return BluetoothAdapterState.unavailable;
//       case BleState.Unsupported:
//         return BluetoothAdapterState.unauthorized;
//     }
//   }
// }