import 'dart:async';
import 'dart:io';

import 'package:flutter_blue_plus/flutter_blue_plus.dart' as Mobile;
import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

class FlutterBluePlus {
  static Future<void> startScan({
    List<Guid> withServices = const [],
    List<String> withRemoteIds = const [],
    List<String> withNames = const [],
    List<String> withKeywords = const [],
    List<MsdFilter> withMsd = const [],
    List<ServiceDataFilter> withServiceData = const [],
    Duration? timeout,
    Duration? removeIfGone,
    bool continuousUpdates = false,
    int continuousDivisor = 1,
    bool oneByOne = false,
    bool androidLegacy = false,
    AndroidScanMode androidScanMode = AndroidScanMode.lowLatency,
    bool androidUsesFineLocation = false,
  }) async {
    if (Platform.isWindows) {
      return await FlutterBluePlusWindows.startScan(
        withServices: withServices,
        withRemoteIds: withRemoteIds,
        withNames: withNames,
        withKeywords: withKeywords,
        withMsd: withMsd,
        withServiceData: withServiceData,
        timeout: timeout,
        removeIfGone: removeIfGone,
        continuousUpdates:continuousUpdates,
        continuousDivisor:continuousDivisor,
        oneByOne: oneByOne,
        androidLegacy: androidLegacy,
        androidScanMode:androidScanMode,
        androidUsesFineLocation: androidUsesFineLocation,
      );
    }

    return await Mobile.FlutterBluePlus.startScan(
      withServices: withServices,
      withRemoteIds: withRemoteIds,
      withNames: withNames,
      withKeywords: withKeywords,
      withMsd: withMsd,
      withServiceData: withServiceData,
      timeout: timeout,
      removeIfGone: removeIfGone,
      continuousUpdates:continuousUpdates,
      continuousDivisor:continuousDivisor,
      oneByOne: oneByOne,
      androidLegacy: androidLegacy,
      androidScanMode:androidScanMode,
      androidUsesFineLocation: androidUsesFineLocation,
    );
  }

  static Stream<BluetoothAdapterState> get adapterState {
    if (Platform.isWindows) return FlutterBluePlusWindows.adapterState;
    return Mobile.FlutterBluePlus.adapterState;
  }

  static Stream<List<ScanResult>> get scanResults {
    if (Platform.isWindows) return FlutterBluePlusWindows.scanResults;
    return Mobile.FlutterBluePlus.scanResults;
  }

  static bool get isScanningNow {
    if (Platform.isWindows) return FlutterBluePlusWindows.isScanningNow;
    return Mobile.FlutterBluePlus.isScanningNow;
  }

  static Stream<bool> get isScanning {
    if (Platform.isWindows) return FlutterBluePlusWindows.isScanning;
    return Mobile.FlutterBluePlus.isScanning;
  }

  static Future<void> stopScan() async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.stopScan();
    return await Mobile.FlutterBluePlus.stopScan();
  }

  static Future<void> setLogLevel(LogLevel level, {color = true}) async {
    if (Platform.isWindows)
      return FlutterBluePlusWindows.setLogLevel(level, color: color);
    return Mobile.FlutterBluePlus.setLogLevel(level, color: color);
  }

  /// TODO: need to verify
  static LogLevel get logLevel => Mobile.FlutterBluePlus.logLevel;

  static Future<bool> get isSupported async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.isSupported;
    return await Mobile.FlutterBluePlus.isSupported;
  }

  static Future<String> get adapterName async {
    if (Platform.isWindows) return await FlutterBluePlusWindows.adapterName;
    return await Mobile.FlutterBluePlus.adapterName;
  }

  static Future<void> turnOn({int timeout = 60}) async {
    if (Platform.isWindows)
      return await FlutterBluePlusWindows.turnOn(timeout: timeout);
    return await Mobile.FlutterBluePlus.turnOn(timeout: timeout);
  }

  static List<BluetoothDevice> get connectedDevices {
    if (Platform.isWindows) return FlutterBluePlusWindows.connectedDevices;
    return Mobile.FlutterBluePlus.connectedDevices;
  }

  static Future<List<BluetoothDevice>> systemDevices(List<Guid> withServices) async {
    if (Platform.isWindows) return FlutterBluePlusWindows.connectedDevices;
    return await Mobile.FlutterBluePlus.systemDevices(withServices);
  }

  static Future<PhySupport> getPhySupport() {
    return Mobile.FlutterBluePlus.getPhySupport();
  }

  static Future<List<BluetoothDevice>> get bondedDevices async {
    if (Platform.isWindows) return FlutterBluePlusWindows.connectedDevices;
    return await Mobile.FlutterBluePlus.bondedDevices;
  }

  static void cancelWhenScanComplete(StreamSubscription subscription) {
    if (Platform.isWindows) return FlutterBluePlusWindows.cancelWhenScanComplete(subscription);
     return Mobile.FlutterBluePlus.cancelWhenScanComplete(subscription);
  }
}
