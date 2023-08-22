import 'package:flutter_blue_plus/flutter_blue_plus.dart';

extension CharacteristicPropertiesExtension on CharacteristicProperties {
  BmCharacteristicProperties toProto() {
    return BmCharacteristicProperties(
      broadcast: broadcast,
      read: read,
      writeWithoutResponse: writeWithoutResponse,
      write: write,
      notify: notify,
      indicate: indicate,
      authenticatedSignedWrites: authenticatedSignedWrites,
      extendedProperties: extendedProperties,
      notifyEncryptionRequired: notifyEncryptionRequired,
      indicateEncryptionRequired: indicateEncryptionRequired,
    );
  }
}
