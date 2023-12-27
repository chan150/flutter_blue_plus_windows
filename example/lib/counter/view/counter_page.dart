import 'dart:developer';

import 'package:flutter/material.dart';
// import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

class CounterPage extends StatelessWidget {
  const CounterPage({super.key});

  @override
  Widget build(BuildContext context) {
    return const CounterView();
  }
}

class CounterView extends StatefulWidget {
  const CounterView({super.key});

  @override
  State<CounterView> createState() => _CounterViewState();
}

class _CounterViewState extends State<CounterView> {
  BluetoothDevice? _device;

  @override
  Widget build(BuildContext context) {
    return SafeArea(
      child: Scaffold(
        body: Column(
          children: [
            StreamBuilder(
              stream:
                  Stream.periodic(const Duration(seconds: 2), (_) => _device),
              builder: (context, snapshot) {
                return Text('device : ${snapshot.data}');
              },
            ),
            StreamBuilder(
              stream: _device?.connectionState,
              builder: (context, snapshot) {
                print('Connection state : ${snapshot.data}');
                return Text('Connection state : ${snapshot.data}');
              },
            ),
            // StreamBuilder(
            //   stream: WinBle.connectionStream,
            //   builder: (context, snapshot) {
            //     return Text(snapshot.data.toString());
            //   },
            // ),
          ],
        ),
        floatingActionButton: Column(
          mainAxisAlignment: MainAxisAlignment.end,
          crossAxisAlignment: CrossAxisAlignment.end,
          children: [
            FloatingActionButton(
              onPressed: () async {
                final list = await FlutterBluePlus.systemDevices;
                print(list);
              },
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final seen = <DeviceIdentifier>{};
                final devices = <ScanResult>[];
                FlutterBluePlus.scanResults.listen(
                  (results) {
                    for (ScanResult r in results) {
                      if (seen.contains(r.device.remoteId) == false) {
                        seen.add(r.device.remoteId);
                        devices.add(r);
                      }
                    }
                  },
                );

                // Start scanning
                // await FlutterBluePlus.startScan(
                //     timeout: const Duration(seconds: 2));
                await FlutterBluePlus.startScan();

                await Future.delayed(const Duration(seconds: 3));

                // Stop scanning
                await FlutterBluePlus.stopScan();

                print(devices);

                devices
                    .where((e) => e.device.platformName.startsWith('HEH001'))
                    .firstOrNull
                    ?.device
                    .connect();
              },
              child: const Icon(Icons.bluetooth),
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final seen = <DeviceIdentifier>{};
                final devices = <ScanResult>[];
                FlutterBluePlus.scanResults.listen(
                  (results) {
                    for (ScanResult r in results) {
                      if (seen.contains(r.device.remoteId) == false) {
                        seen.add(r.device.remoteId);
                        devices.add(r);
                      }
                    }
                  },
                );

                // Start scanning
                await FlutterBluePlus.startScan();

                await Future.delayed(const Duration(seconds: 3));

                // Stop scanning
                await FlutterBluePlus.stopScan();

                print(
                  devices
                      .where((e) => e.device.platformName.startsWith('C-Click'))
                      .map((e) => e.toString())
                      .join('\n'),
                );
              },
              child: const Icon(
                Icons.bluetooth,
                color: Colors.red,
              ),
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final connected = await FlutterBluePlus.systemDevices;
                print(connected);
                connected
                    .where((e) => e.platformName.startsWith('HEH001'))
                    .lastOrNull
                    ?.disconnect();
              },
              child: const Icon(Icons.bluetooth_disabled),
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final connected = await FlutterBluePlus.systemDevices;
                final s = await connected
                    .where((e) => e.platformName.startsWith('HEH001'))
                    .lastOrNull
                    ?.discoverServices();
                print(s);
              },
              child: const Icon(Icons.insert_emoticon_rounded),
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final connected = await FlutterBluePlus.systemDevices;
                print(connected);
                connected
                    .where((e) => e.platformName.startsWith('C-Click'))
                    .lastOrNull
                    ?.disconnect();
              },
              child: const Icon(
                Icons.bluetooth_disabled,
                color: Colors.red,
              ),
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final devices = await FlutterBluePlus.systemDevices;
                print(devices);

                if (devices.isNotEmpty) {
                  _device = devices.first;
                } else {
                  _device = null;
                }
                setState(() {});
              },
              child: const Icon(Icons.refresh),
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final _device = this._device;
                if (_device != null) {
                  // log(_device.toString());
                  log((await _device.discoverServices()).join('\n'));
                  // log(_device.toString());
                }
              },
              child: const Icon(Icons.refresh),
            ),
          ],
        ),
      ),
    );
  }
}
