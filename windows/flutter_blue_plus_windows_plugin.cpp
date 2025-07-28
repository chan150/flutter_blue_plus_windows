#include "flutter_blue_plus_windows_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <Windows.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Radios.h>

#include <memory>
#include <sstream>
#include <iomanip>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Radios;

namespace flutter_blue_plus_windows {

namespace {
std::string to_string(const winrt::hstring& hstr) {
    return winrt::to_string(hstr);
}

template <typename T>
T from_value(const flutter::EncodableValue* value) {
    if (auto* ptr = std::get_if<T>(value)) {
        return *ptr;
    }
    return T{};
}
}  // namespace

void FlutterBluePlusWindowsPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
    auto plugin = std::make_unique<FlutterBluePlusWindowsPlugin>(registrar);

    plugin->channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
        registrar->messenger(), "flutter_blue_plus/methods",
        &flutter::StandardMethodCodec::GetInstance());

    plugin->channel_->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
}

FlutterBluePlusWindowsPlugin::FlutterBluePlusWindowsPlugin(flutter::PluginRegistrarWindows* registrar)
    : registrar_(registrar) {
    watcher_.ScanningMode(BluetoothLEScanningMode::Active);
    received_token_ = watcher_.Received(
        { this, &FlutterBluePlusWindowsPlugin::OnAdvertisementReceived });
    stopped_token_ = watcher_.Stopped(
        { this, &FlutterBluePlusWindowsPlugin::OnAdvertisementStopped });
}

FlutterBluePlusWindowsPlugin::~FlutterBluePlusWindowsPlugin() {
    watcher_.Stopped(stopped_token_);
    watcher_.Received(received_token_);
}

void FlutterBluePlusWindowsPlugin::OnAdvertisementReceived(
    const BluetoothLEAdvertisementWatcher&,
    const BluetoothLEAdvertisementReceivedEventArgs& args) {
    if (channel_) {
        uint64_t addr = args.BluetoothAddress();
        std::stringstream stream;
        stream << std::hex << std::uppercase << std::setfill('0') 
               << std::setw(2) << ((addr >> 40) & 0xFF) << ":"
               << std::setw(2) << ((addr >> 32) & 0xFF) << ":"
               << std::setw(2) << ((addr >> 24) & 0xFF) << ":"
               << std::setw(2) << ((addr >> 16) & 0xFF) << ":"
               << std::setw(2) << ((addr >> 8) & 0xFF) << ":"
               << std::setw(2) << (addr & 0xFF);
        std::string remote_id = stream.str();

        flutter::EncodableMap map;
        map[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);

        if (!args.Advertisement().LocalName().empty()) {
            map[flutter::EncodableValue("platform_name")] =
                flutter::EncodableValue(to_string(args.Advertisement().LocalName()));
        }

        map[flutter::EncodableValue("rssi")] =
            flutter::EncodableValue(static_cast<int32_t>(args.RawSignalStrengthInDBm()));

        // TODO: Populate other fields like manufacturer data, service UUIDs, etc.

        flutter::EncodableMap response;
        response[flutter::EncodableValue("advertisements")] = flutter::EncodableList{ flutter::EncodableValue(map) };
        channel_->InvokeMethod("OnScanResponse", std::make_unique<flutter::EncodableValue>(response));
    }
}

void FlutterBluePlusWindowsPlugin::OnAdvertisementStopped(
    const BluetoothLEAdvertisementWatcher&,
    const BluetoothLEAdvertisementWatcherStoppedEventArgs& args) {
    // This can be used to notify Flutter that the scan has stopped, if needed.
}

fire_and_forget GetSystemDevicesAsync(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    try {
        auto selector = BluetoothDevice::GetDeviceSelectorFromPairingState(true);
        DeviceInformationCollection deviceInfoCollection = co_await DeviceInformation::FindAllAsync(selector);
        
        flutter::EncodableMap response = {};
        flutter::EncodableList deviceList;

        for (auto&& deviceInfo : deviceInfoCollection) {
            try {
                flutter::EncodableMap deviceMap = {};
                deviceMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(to_string(deviceInfo.Id()));
                deviceMap[flutter::EncodableValue("platform_name")] = flutter::EncodableValue(to_string(deviceInfo.Name()));
                deviceList.push_back(flutter::EncodableValue(deviceMap));
            }
            catch (const hresult_error& e) {
                OutputDebugStringW(L"Error processing device: ");
                OutputDebugStringW(e.message().c_str());
                OutputDebugStringW(L"\n");
            }
        }

        response[flutter::EncodableValue("devices")] = deviceList;
        result->Success(flutter::EncodableValue(response));
    }
    catch (const hresult_error& e) {
        result->Error("getSystemDevices", to_string(e.message()));
    }
    catch (const std::exception& e) {
        result->Error("getSystemDevices", e.what());
    }
    co_return;
}

fire_and_forget GetAdapterStateAsync(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    try {
        auto radios = co_await Radio::GetRadiosAsync();
        std::string adapter_name = "";
        int adapter_state = 0;
        for (auto radio : radios) {
            if (radio.Kind() != RadioKind::Bluetooth) continue;
            adapter_name = to_string(radio.Name());
            switch (radio.State()) {
            case RadioState::On:
                adapter_state = 4; // Corresponds to AdapterState::On
                break;
            case RadioState::Off:
                adapter_state = 6; // Corresponds to AdapterState::Off
                break;
            default:
                adapter_state = 0; // Corresponds to AdapterState::Unknown
                break;
            }
            break;
        }
        flutter::EncodableMap response = {};
        response[flutter::EncodableValue("adapter_state")] = flutter::EncodableValue(adapter_state);
        response[flutter::EncodableValue("adapter_name")] = flutter::EncodableValue(adapter_name);
        result->Success(flutter::EncodableValue(response));
    }
    catch (const hresult_error& e) {
        result->Error("getAdapterState", to_string(e.message()));
    }
    catch (const std::exception& e) {
        result->Error("getAdapterState", e.what());
    }
    co_return;
}

void FlutterBluePlusWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const auto& method = method_call.method_name();

    if (method == "startScan") {
        watcher_.Start();
        result->Success(flutter::EncodableValue(true));
        return;
    }

    if (method == "stopScan") {
        watcher_.Stop();
        result->Success(flutter::EncodableValue(true));
        return;
    }

    if (method == "getSystemDevices") {
        try {
            GetSystemDevicesAsync(std::move(result));
        }
        catch (const std::exception& e) {
            result->Error("getSystemDevices", e.what());
        }
        return;
    }

    if (method == "getAdapterState") {
        try {
            GetAdapterStateAsync(std::move(result));
        }
        catch (const std::exception& e) {
            result->Error("getAdapterState", e.what());
        }
        return;
    }

    if (method == "connectedCount") {
        result->Success(0);
        return;
    }

    if (method == "turnOn") {
        result->Success(flutter::EncodableValue(false));
        return;
    }

    if (method == "turnOff") {
        result->Success(flutter::EncodableValue(true));
        return;
    }

    result->NotImplemented();
}

}  // namespace flutter_blue_plus_windows
