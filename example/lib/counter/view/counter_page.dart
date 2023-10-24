import 'dart:developer';

import 'package:flutter/material.dart';
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
            StreamBuilder(
              stream: WinBle.connectionStream,
              builder: (context, snapshot) {
                return Text(snapshot.data.toString());
              },
            ),
            // StreamBuilder(
            //   // stream: WinBle.connectionStream,
            //   stream: WinBle.connectionStreamOf('cc:17:8a:a0:2a:18'),
            //   builder: (context, snapshot) {
            //     return Text(snapshot.data.toString());
            //   },
            // ),
            // StreamBuilder(
            //   // stream: WinBle.connectionStream,
            //   stream: WinBle.connectionStreamOf('d7:d4:7c:61:1d:c7'),
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
                var isFinished = false;
                Set<DeviceIdentifier> seen = {};
                FlutterBluePlus.scanResults.listen(
                  (results) {
                    for (ScanResult r in results) {
                      if (isFinished) return;
                      if (seen.contains(r.device.remoteId) == false) {
                        seen.add(r.device.remoteId);
                      }

                      if (r.device.platformName.startsWith('HEH001')) {
                        isFinished = true;
                        r.device.connect();
                      }
                    }
                  },
                );

                // Start scanning
                await FlutterBluePlus.startScan();

                await Future.delayed(const Duration(seconds: 3));

                // Stop scanning
                await FlutterBluePlus.stopScan();
                print(seen);
              },
              child: const Icon(Icons.bluetooth),
            ),
            const SizedBox(height: 8),
            FloatingActionButton(
              onPressed: () async {
                final connected = await FlutterBluePlus.systemDevices;
                print(connected);
                connected
                    .where(
                        (element) => element.platformName.startsWith('HEH001'))
                    .lastOrNull
                    ?.disconnect();
                // await WinBle.disconnect('cc:17:8a:a0:2a:18'.toLowerCase());
              },
              child: const Icon(Icons.bluetooth_disabled),
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
