#ifndef FLUTTER_PLUGIN_FLUTTER_BLUE_PLUS_WINDOWS_PLUGIN_H_
#define FLUTTER_PLUGIN_FLUTTER_BLUE_PLUS_WINDOWS_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Foundation.h>

#include <memory>
#include <string>
#include <vector>
#include <utility>

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

  // Using vector of pairs to avoid std::map issues with non-default-constructible WinRT types
  std::vector<std::pair<std::string, winrt::Windows::Foundation::IInspectable>> connected_devices_{};

  void OnAdvertisementReceived(
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher&,
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs&);

  void OnAdvertisementStopped(
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher&,
      const winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcherStoppedEventArgs&);
  
  winrt::fire_and_forget GetSystemDevicesAsync(
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  winrt::fire_and_forget ConnectAsync(
      std::string remote_id,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

  void OnConnectionStatusChanged(
    const winrt::Windows::Devices::Bluetooth::BluetoothLEDevice&,
    const winrt::Windows::Foundation::IInspectable&);

  std::string uint64_to_mac_string(uint64_t addr);
};

}  // namespace flutter_blue_plus_windows

#endif  // FLUTTER_PLUGIN_FLUTTER_BLUE_PLUS_WINDOWS_PLUGIN_H_
