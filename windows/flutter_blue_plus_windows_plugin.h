#ifndef FLUTTER_PLUGIN_FLUTTER_BLUE_PLUS_WINDOWS_PLUGIN_H_
#define FLUTTER_PLUGIN_FLUTTER_BLUE_PLUS_WINDOWS_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>

#include <memory>

namespace flutter_blue_plus_windows {

class FlutterBluePlusWindowsPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  FlutterBluePlusWindowsPlugin(flutter::PluginRegistrarWindows* registrar);

  ~FlutterBluePlusWindowsPlugin();

  // Disallow copy and assign.
  FlutterBluePlusWindowsPlugin(const FlutterBluePlusWindowsPlugin&) = delete;
  FlutterBluePlusWindowsPlugin& operator=(const FlutterBluePlusWindowsPlugin&) = delete;

 private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  flutter::PluginRegistrarWindows* registrar_;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;

  winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher watcher_{};
  winrt::event_token received_token_{};
  winrt::event_token stopped_token_{};

  void OnAdvertisementReceived(
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher&,
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs&);

  void OnAdvertisementStopped(
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher&,
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcherStoppedEventArgs&);
};

}  // namespace flutter_blue_plus_windows

#endif  // FLUTTER_PLUGIN_FLUTTER_BLUE_PLUS_WINDOWS_PLUGIN_H_