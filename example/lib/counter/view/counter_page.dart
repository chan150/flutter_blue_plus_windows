import 'package:example/counter/counter.dart';
import 'package:example/l10n/l10n.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_blue_plus_windows/flutter_blue_plus_windows.dart';

class CounterPage extends StatelessWidget {
  const CounterPage({super.key});

  @override
  Widget build(BuildContext context) {
    return BlocProvider(
      create: (_) => CounterCubit(),
      child: const CounterView(),
    );
  }
}

class CounterView extends StatelessWidget {
  const CounterView({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: StreamBuilder(
        stream: FlutterBluePlus.isScanning,
        builder: (context, snapshot) {
          return Text(snapshot.data.toString());
        },
      ),
      floatingActionButton: Column(
        mainAxisAlignment: MainAxisAlignment.end,
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          FloatingActionButton(
            onPressed: () async {
              // final bd = BluetoothDevice.fromId('');
              // bd.connect();
              // await FlutterBluePlus.startScan(
              //   timeout: const Duration(seconds: 4),
              // );
              // final a = FlutterBluePlus.scan();
              // await for (final s in a) {
              //   print(s);
              // }

              print('1');
              await FlutterBluePlus.startScan(timeout: Duration(seconds: 4));

              print('2');
              var subscription = FlutterBluePlus.scanResults.listen((results) {
                for (final r in results) {
                  print(
                    '==> ${r.device} ${r.rssi} ${r.advertisementData} ${r.timeStamp}',
                  );
                }
              });
              print('3');

              await FlutterBluePlus.stopScan();
              print('4');
              subscription.cancel();
              print('5');

              // final connected = await FlutterBluePlus.turnOn();
              // print(connected);
            },
            child: const Icon(Icons.add),
          ),
          const SizedBox(height: 8),
          FloatingActionButton(
            onPressed: () async {
              await FlutterBluePlus.stopScan();
              // final connected = await FlutterBluePlus.turnOff();
              // print(connected);
            },
            child: const Icon(Icons.remove),
          ),
        ],
      ),
    );
  }
}
