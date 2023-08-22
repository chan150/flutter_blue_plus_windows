import 'package:flutter_blue_plus/flutter_blue_plus.dart';

class BluetoothServiceWindows extends BluetoothService {
  final DeviceIdentifier remoteId;
  final Guid serviceUuid;
  final bool isPrimary;
  final List<BluetoothCharacteristic> characteristics;
  final List<BluetoothService> includedServices;

  BluetoothServiceWindows({
    required this.remoteId,
    required this.serviceUuid,
    required this.isPrimary,
    required this.characteristics,
    required this.includedServices,
  }) : super.fromProto(
          BmBluetoothService(
            serviceUuid: serviceUuid,
            remoteId: remoteId.str,
            isPrimary: isPrimary,
            characteristics: [
              for (final c in characteristics)
                BmBluetoothCharacteristic(
                  remoteId: remoteId.str,
                  serviceUuid: serviceUuid,
                  secondaryServiceUuid: serviceUuid,
                  characteristicUuid: c.uuid,
                  descriptors: c.descriptors
                      .map(
                        (d) => BmBluetoothDescriptor(
                          remoteId: remoteId.str,
                          serviceUuid: serviceUuid,
                          characteristicUuid: c.uuid,
                          descriptorUuid: d.uuid,
                          value: d.lastValue,
                        ),
                      )
                      .toList(),
                  properties: BmCharacteristicProperties(
                    broadcast: c.properties.broadcast,
                    read: c.properties.read,
                    writeWithoutResponse: c.properties.writeWithoutResponse,
                    write: c.properties.write,
                    notify: c.properties.notify,
                    indicate: c.properties.indicate,
                    authenticatedSignedWrites:
                        c.properties.authenticatedSignedWrites,
                    extendedProperties: c.properties.extendedProperties,
                    notifyEncryptionRequired:
                        c.properties.notifyEncryptionRequired,
                    indicateEncryptionRequired:
                        c.properties.indicateEncryptionRequired,
                  ),
                  value: c.lastValue,
                ),
            ],
            includedServices: includedServices
                .map(
                  (s) => BmBluetoothService(
                    serviceUuid: serviceUuid,
                    remoteId: remoteId.str,
                    isPrimary: isPrimary,
                    characteristics: [
                      for (final c in characteristics)
                        BmBluetoothCharacteristic(
                          remoteId: remoteId.str,
                          serviceUuid: serviceUuid,
                          secondaryServiceUuid: serviceUuid,
                          characteristicUuid: c.uuid,
                          descriptors: c.descriptors
                              .map(
                                (d) => BmBluetoothDescriptor(
                              remoteId: remoteId.str,
                              serviceUuid: serviceUuid,
                              characteristicUuid: c.uuid,
                              descriptorUuid: d.uuid,
                              value: d.lastValue,
                            ),
                          )
                              .toList(),
                          properties: BmCharacteristicProperties(
                            broadcast: c.properties.broadcast,
                            read: c.properties.read,
                            writeWithoutResponse: c.properties.writeWithoutResponse,
                            write: c.properties.write,
                            notify: c.properties.notify,
                            indicate: c.properties.indicate,
                            authenticatedSignedWrites:
                            c.properties.authenticatedSignedWrites,
                            extendedProperties: c.properties.extendedProperties,
                            notifyEncryptionRequired:
                            c.properties.notifyEncryptionRequired,
                            indicateEncryptionRequired:
                            c.properties.indicateEncryptionRequired,
                          ),
                          value: c.lastValue,
                        ),
                    ],
                    includedServices: [],
                  ),
                )
                .toList(),
          ),
        );
}
