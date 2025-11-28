#pragma warning(disable : 4819)

#include "flutter_blue_plus_windows_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <Windows.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>

#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <exception>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Radios;

namespace flutter_blue_plus_windows {

namespace utils {
std::string to_string(const winrt::hstring& hstr) {
    return winrt::to_string(hstr);
}

template <typename T>
T from_value(const flutter::EncodableValue* value) {
    if (auto* ptr = std::get_if<T>(value)) {
        return *ptr;
    }
    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        if (auto* ptr64 = std::get_if<int64_t>(value)) {
            return static_cast<T>(*ptr64);
        }
        if (auto* ptr32 = std::get_if<int32_t>(value)) {
            return static_cast<T>(*ptr32);
        }
    }
    return T{};
}

uint64_t mac_to_uint64(const std::string& mac_address) {
    std::stringstream ss(mac_address);
    uint64_t result = 0;
    for (int i = 0; i < 6; ++i) {
        long long byte;
        ss >> std::hex >> byte;
        result = (result << 8) | byte;
        if (i < 5) {
            ss.ignore();
        }
    }
    return result;
}

std::vector<uint8_t> to_vector(const winrt::Windows::Storage::Streams::IBuffer& buffer) {
    auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
    std::vector<uint8_t> data(reader.UnconsumedBufferLength());
    if (!data.empty()) {
        reader.ReadBytes(data);
    }
    return data;
}

std::string to_uuid_string(const winrt::guid& uuid) {
    std::string uuid_str_with_braces = winrt::to_string(winrt::to_hstring(uuid));
    std::string full_uuid;

    // Remove curly braces if present
    if (uuid_str_with_braces.length() >= 2 && uuid_str_with_braces.front() == '{' && uuid_str_with_braces.back() == '}') {
        full_uuid = uuid_str_with_braces.substr(1, uuid_str_with_braces.length() - 2);
    } else {
        full_uuid = uuid_str_with_braces;
    }

    // Ensure lowercase for consistent comparison and output
    std::transform(full_uuid.begin(), full_uuid.end(), full_uuid.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    // Check for standard 16-bit UUID pattern: 0000xxxx-0000-1000-8000-00805f9b34fb
    // Note: The length check should now be for 36 characters (without braces)
    if (full_uuid.length() == 36 &&
        full_uuid.substr(0, 4) == "0000" &&
        full_uuid.substr(8) == "-0000-1000-8000-00805f9b34fb") {
        return full_uuid.substr(4, 4); // Return short form like "180d"
    }
    // For other UUIDs, return the full 128-bit version
    return full_uuid;
}

// Helper to parse UUID string to winrt::guid
winrt::guid parse_uuid(const std::string& uuid_str) {
    std::string full_uuid = uuid_str;
    if (full_uuid.length() == 4) {
        full_uuid = "0000" + full_uuid + "-0000-1000-8000-00805f9b34fb";
    }
    
    // Add braces if not present for GUID parsing
    if (full_uuid.front() != '{') {
        full_uuid = "{" + full_uuid + "}";
    }

    // Convert std::string to wstring for GUIDFromString
    std::wstring wstr(full_uuid.begin(), full_uuid.end());
    GUID guid;
    if (SUCCEEDED(IIDFromString(wstr.c_str(), &guid))) {
        return winrt::guid(guid);
    }
    return winrt::guid(); // Return empty GUID on failure
}

}  // namespace utils

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
    
    // Switch to UI thread to handle the advertisement and channel method invocation safely
    [this, args]() -> winrt::fire_and_forget {
        try {
            co_await ui_thread_;
            
            // if (args.IsScanResponse()) return;
            if (channel_) {
                std::string remote_id = uint64_to_mac_string(args.BluetoothAddress());
                
                rssi_cache_[remote_id] = static_cast<int32_t>(args.RawSignalStrengthInDBm());

                // Ensure entry exists in cache
                if (scan_results_cache_.find(remote_id) == scan_results_cache_.end()) {
                     scan_results_cache_[remote_id] = {}; // Initialize with empty map
                }
                auto& map = scan_results_cache_[remote_id];

                // Always update these fields
                map[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                map[flutter::EncodableValue("rssi")] =
                    flutter::EncodableValue(static_cast<int32_t>(args.RawSignalStrengthInDBm()));
                
                // connectable (Merge: once 1, always 1)
                int connectable = args.IsConnectable() ? 1 : 0;
                if (connectable == 0) {
                     auto it = map.find(flutter::EncodableValue("connectable"));
                     if (it != map.end()) {
                         if (auto* val = std::get_if<int>(&it->second)) {
                             if (*val == 1) connectable = 1;
                         }
                     }
                }
                map[flutter::EncodableValue("connectable")] = flutter::EncodableValue(connectable);

                auto advertisement = args.Advertisement();

                // adv_name & platform_name (Merge if not empty)
                std::string localNameStr = utils::to_string(advertisement.LocalName());
                if (!localNameStr.empty()) {
                    map[flutter::EncodableValue("adv_name")] = flutter::EncodableValue(localNameStr);
                    map[flutter::EncodableValue("platform_name")] = flutter::EncodableValue(localNameStr);
                }

                // tx_power_level (Update if present)
                if (args.TransmitPowerLevelInDBm() != nullptr) {
                    map[flutter::EncodableValue("tx_power_level")] =
                        flutter::EncodableValue(static_cast<int32_t>(args.TransmitPowerLevelInDBm().Value()));
                }

                // appearance (Update if present)
                for (const auto& section : advertisement.DataSections()) {
                    if (section.DataType() == 0x19) { // Appearance
                        auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(section.Data());
                        reader.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian); // Set to Little Endian
                        uint16_t appearance_value;
                        appearance_value = reader.ReadUInt16();
                        map[flutter::EncodableValue("appearance")] = flutter::EncodableValue(static_cast<int32_t>(appearance_value));
                        break;
                    }
                }

                // manufacturer_data (Merge)
                if (advertisement.ManufacturerData().Size() > 0) {
                    flutter::EncodableMap msd_map; // Use default type or std::map<EncodableValue, EncodableValue>
                    
                    // Retrieve existing map if it exists
                    auto it = map.find(flutter::EncodableValue("manufacturer_data"));
                    if (it != map.end()) {
                        if (auto* existing = std::get_if<flutter::EncodableMap>(&it->second)) {
                            msd_map = *existing;
                        }
                    }

                    for (const auto& msd : advertisement.ManufacturerData()) {
                        msd_map[flutter::EncodableValue(static_cast<int64_t>(msd.CompanyId()))] = 
                            flutter::EncodableValue(utils::to_vector(msd.Data()));
                    }
                    map[flutter::EncodableValue("manufacturer_data")] = msd_map;
                }

                // service_data (Merge)
                flutter::EncodableMap service_data_map;
                // Retrieve existing map if it exists
                auto it_sd = map.find(flutter::EncodableValue("service_data"));
                if (it_sd != map.end()) {
                    if (auto* existing = std::get_if<flutter::EncodableMap>(&it_sd->second)) {
                        service_data_map = *existing;
                    }
                }

                bool has_new_service_data = false;
                for (const auto& section : advertisement.GetSectionsByType(0x16)) {
             auto buffer = section.Data();
             if (buffer.Length() >= 2) {
                 has_new_service_data = true;
                 auto all_data = utils::to_vector(buffer);
                 uint16_t uuid16 = (all_data[1] << 8) | all_data[0];
                 std::stringstream ss;
                 ss << std::hex << std::setfill('0') << std::setw(4) << uuid16;
                 std::string uuid_str = "0000" + ss.str() + "-0000-1000-8000-00805f9b34fb";
                 std::vector<uint8_t> data_vec(all_data.begin() + 2, all_data.end());
                 service_data_map[flutter::EncodableValue(uuid_str)] = flutter::EncodableValue(data_vec);
             }
        }
        if (has_new_service_data || !service_data_map.empty()) {
            map[flutter::EncodableValue("service_data")] = service_data_map;
        }

                // service_uuids (Merge)
                if (advertisement.ServiceUuids().Size() > 0) {
                    flutter::EncodableList service_uuids_list;
                     // Retrieve existing list if it exists
                    auto it_u = map.find(flutter::EncodableValue("service_uuids"));
                    if (it_u != map.end()) {
                        if (auto* existing = std::get_if<flutter::EncodableList>(&it_u->second)) {
                            service_uuids_list = *existing;
                        }
                    }

                    for (const auto& uuid : advertisement.ServiceUuids()) {
                         std::string uuid_str = utils::to_uuid_string(uuid);
                         bool found = false;
                         for(const auto& existing_val : service_uuids_list) {
                             if (auto* s = std::get_if<std::string>(&existing_val)) {
                                 if (*s == uuid_str) {
                                     found = true;
                                     break;
                                 }
                             }
                         }
                         if (!found) {
                             service_uuids_list.push_back(flutter::EncodableValue(uuid_str));
                         }
                    }
                    map[flutter::EncodableValue("service_uuids")] = service_uuids_list;
                }

                flutter::EncodableMap response;
                // map is std::map<EncodableValue, EncodableValue> which is implicitly EncodableMap
                response[flutter::EncodableValue("advertisements")] = flutter::EncodableList{ flutter::EncodableValue(map) };
                channel_->InvokeMethod("OnScanResponse", std::make_unique<flutter::EncodableValue>(response));
            }
        } catch (...) {
            // Ignore errors in callback
        }
    }();
}

void FlutterBluePlusWindowsPlugin::OnAdvertisementStopped(
    const BluetoothLEAdvertisementWatcher&,
    const BluetoothLEAdvertisementWatcherStoppedEventArgs&) {
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::GetSystemDevicesAsync(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    std::string error_msg;
    try {
        auto selector = BluetoothDevice::GetDeviceSelectorFromPairingState(true);
        DeviceInformationCollection deviceInfoCollection = co_await DeviceInformation::FindAllAsync(selector);

        flutter::EncodableMap response = {};
        flutter::EncodableList deviceList;

        for (auto&& deviceInfo : deviceInfoCollection) {
            try {
                auto bleDevice = co_await BluetoothLEDevice::FromIdAsync(deviceInfo.Id());
                if (!bleDevice) {
                    continue;
                }

                std::string remote_id = uint64_to_mac_string(bleDevice.BluetoothAddress());

                // Check actual system connection status
                bool is_connected = (bleDevice.ConnectionStatus() == BluetoothConnectionStatus::Connected);
                // Also could check internal list, but system status is more accurate for "GetSystemDevices"

                flutter::EncodableMap deviceMap = {};
                deviceMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                deviceMap[flutter::EncodableValue("platform_name")] = flutter::EncodableValue(utils::to_string(deviceInfo.Name()));
                deviceMap[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(is_connected ? 1 : 0);
                deviceList.push_back(flutter::EncodableValue(deviceMap));
            }
            catch (const hresult_error& e) {
                OutputDebugStringW(L"Error processing device: ");
                OutputDebugStringW(e.message().c_str());
                OutputDebugStringW(L"\n");
            }
        }

        response[flutter::EncodableValue("devices")] = deviceList;
        
        co_await ui_thread_;
        result->Success(flutter::EncodableValue(response));
    }
    catch (const std::exception& e) {
        error_msg = e.what();
    }
    catch (...) {
        error_msg = "Unknown error occurred";
    }

    if (!error_msg.empty()) {
        co_await ui_thread_;
        result->Error("getSystemDevices", error_msg);
    }
}

fire_and_forget GetAdapterStateAsync(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    // Note: GetAdapterStateAsync is a free function, not a member, so it cannot access ui_thread_.
    // However, it can just use winrt::apartment_context locally.
    winrt::apartment_context ui_thread;
    std::string error_msg;
    try {
        auto radios = co_await Radio::GetRadiosAsync();
        std::string adapter_name = "";
        int adapter_state = 0;
        for (auto radio : radios) {
            if (radio.Kind() != RadioKind::Bluetooth) continue;
            adapter_name = utils::to_string(radio.Name());
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
        
        co_await ui_thread;
        result->Success(flutter::EncodableValue(response));
    }
    catch (const std::exception& e) {
        error_msg = e.what();
    }
    catch (...) {
        error_msg = "Unknown error occurred";
    }
    
    if (!error_msg.empty()) {
        co_await ui_thread;
        result->Error("getAdapterState", error_msg);
    }
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::ConnectAsync(
    std::string remote_id,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    
    std::string error_msg;
    try {
        uint64_t bluetoothAddress = utils::mac_to_uint64(remote_id);

        // Check if already connected
        auto it_existing = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
        if (it_existing != connected_devices_.end()) {
             // Already connected, ensure event is sent and return success
             co_await ui_thread_;
             flutter::EncodableMap connection_state;
             connection_state[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
             connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(1); 
             channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));
             result->Success(flutter::EncodableValue(true));
             co_return;
        }

        auto device = co_await BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);

        if (device) {
            auto it = std::find_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
            if (it == currently_connecting_devices_.end()) {
                 currently_connecting_devices_.emplace_back(remote_id, device);
            }

            device.ConnectionStatusChanged({ this, &FlutterBluePlusWindowsPlugin::OnConnectionStatusChanged });
            
            // Trigger connection by getting GATT services
            // Use Cached to avoid connection drops
            auto gatt_result = co_await device.GetGattServicesAsync(BluetoothCacheMode::Cached);
            
            if (gatt_result.Status() != GattCommunicationStatus::Success) {
                gatt_result = co_await device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
            }

            if (gatt_result.Status() == GattCommunicationStatus::Success) {
                 co_await ui_thread_;

                 // Explicitly update connected_devices_ to avoid race conditions with OnConnectionStatusChanged
                 auto it_connected = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                    [&](const auto& pair) { return pair.first == remote_id; });
                 if (it_connected == connected_devices_.end()) {
                     connected_devices_.emplace_back(remote_id, device);
                 }
                 
                 // Remove from connecting list
                 auto it_connecting = std::find_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                        [&](const auto& pair) { return pair.first == remote_id; });
                 if (it_connecting != currently_connecting_devices_.end()) {
                     currently_connecting_devices_.erase(it_connecting);
                 }

                 // Send Connected Event explicitly to ensure Dart state updates
                 flutter::EncodableMap connection_state;
                 connection_state[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                 connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(1); 
                 channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));

                 result->Success(flutter::EncodableValue(true));
            } else {
                 // Cleanup if it was moved to connected_devices_ by the event handler
                 co_await ui_thread_;
                 auto it_connected = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                    [&](const auto& pair) { return pair.first == remote_id; });
                 
                 if (it_connected != connected_devices_.end()) {
                     auto d = it_connected->second.as<BluetoothLEDevice>();
                     if (d) d.Close();
                     connected_devices_.erase(it_connected);

                     flutter::EncodableMap connection_state;
                     connection_state[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                     connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(0); 
                     channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));
                 }

                 currently_connecting_devices_.erase(
                    std::remove_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                        [&](const auto& pair) { return pair.first == remote_id; }),
                    currently_connecting_devices_.end());
                error_msg = "Failed to connect: " + std::to_string((int)gatt_result.Status());
            }
        }
        else {
            error_msg = "Device not found.";
        }
    }
    catch (const std::exception& e) {
        error_msg = e.what();
    }
    catch (...) {
        error_msg = "Unknown error occurred";
    }

    if (!error_msg.empty()) {
        co_await ui_thread_;
        result->Error("connect", error_msg);
    }
}

std::string FlutterBluePlusWindowsPlugin::uint64_to_mac_string(uint64_t addr) {
    std::stringstream stream;
    stream << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << ((addr >> 40) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 32) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 24) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 16) & 0xFF) << ":"
        << std::setw(2) << ((addr >> 8) & 0xFF) << ":"
        << std::setw(2) << (addr & 0xFF);
    return stream.str();
}

void FlutterBluePlusWindowsPlugin::OnConnectionStatusChanged(
    const BluetoothLEDevice& device,
    const IInspectable&) {
    
    // Spawn a fire_and_forget coroutine to safely switch to UI thread before invoking method channel
    [&](BluetoothLEDevice d) -> winrt::fire_and_forget {
        // Switch to UI thread
        co_await ui_thread_;

        std::string remote_id = uint64_to_mac_string(d.BluetoothAddress());
        flutter::EncodableMap connection_state;
        connection_state[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);

        if (d.ConnectionStatus() == BluetoothConnectionStatus::Connected) {

            // Check if already in connected list
             auto it_connected = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
             
             if (it_connected == connected_devices_.end()) {
                  auto it_connecting = std::find_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                    [&](const auto& pair) { return pair.first == remote_id; });

                  if(it_connecting != currently_connecting_devices_.end()) {
                       connected_devices_.emplace_back(remote_id, d);
                       currently_connecting_devices_.erase(it_connecting);
                  }
             }
            
            connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(1); // connected
            channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));

        } else if (d.ConnectionStatus() == BluetoothConnectionStatus::Disconnected) {
           
            auto it_connected = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });

            if (it_connected != connected_devices_.end()) {
                connected_devices_.erase(it_connected);
            }

            connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(0); // disconnected
            channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));
        }
    }(device);
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::DiscoverServicesAsync(
    std::string remote_id,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    
    auto result_ptr = std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>(std::move(result));
    std::string error_msg;

    try {
        BluetoothLEDevice device = nullptr;
        {
             auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
             if (it != connected_devices_.end()) {
                 device = it->second.as<BluetoothLEDevice>();
             }
        }

        if (!device) {
             result_ptr->Error("discoverServices", "device is disconnected");
             co_return;
        }

        co_await winrt::resume_background();

        auto servicesResult = co_await device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
             co_await ui_thread_;
             result_ptr->Error("discoverServices", "GetGattServicesAsync failed");
             co_return;
        }

        flutter::EncodableList servicesList;

        for (auto service : servicesResult.Services()) {
             flutter::EncodableMap serviceMap;
             std::string serviceUuid = utils::to_uuid_string(service.Uuid());
             serviceMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
             serviceMap[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(serviceUuid);

             auto charsResult = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
             flutter::EncodableList charsList;
             
             if (charsResult.Status() == GattCommunicationStatus::Success) {
                 for (auto characteristic : charsResult.Characteristics()) {
                     flutter::EncodableMap charMap;
                     std::string charUuid = utils::to_uuid_string(characteristic.Uuid());
                     charMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                     charMap[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(serviceUuid);
                     charMap[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(charUuid);
                     charMap[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(static_cast<int32_t>(characteristic.AttributeHandle()));

                     auto props = characteristic.CharacteristicProperties();
                     flutter::EncodableMap propsMap;
                     propsMap[flutter::EncodableValue("broadcast")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Broadcast)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("read")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Read)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("write_without_response")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::WriteWithoutResponse)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("write")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Write)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("notify")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Notify)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("indicate")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Indicate)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("authenticated_signed_writes")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::AuthenticatedSignedWrites)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("extended_properties")] = flutter::EncodableValue((static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::ExtendedProperties)) ? 1 : 0);
                     propsMap[flutter::EncodableValue("notify_encryption_required")] = flutter::EncodableValue(0);
                     propsMap[flutter::EncodableValue("indicate_encryption_required")] = flutter::EncodableValue(0);

                     charMap[flutter::EncodableValue("properties")] = propsMap;

                     auto descResult = co_await characteristic.GetDescriptorsAsync(BluetoothCacheMode::Uncached);
                     flutter::EncodableList descList;
                     if (descResult.Status() == GattCommunicationStatus::Success) {
                         for (auto descriptor : descResult.Descriptors()) {
                             flutter::EncodableMap descMap;
                             descMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                             descMap[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(serviceUuid);
                             descMap[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(charUuid);
                             descMap[flutter::EncodableValue("descriptor_uuid")] = flutter::EncodableValue(utils::to_uuid_string(descriptor.Uuid()));
                             descMap[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(static_cast<int32_t>(descriptor.AttributeHandle()));
                             descList.push_back(descMap);
                         }
                     }
                     charMap[flutter::EncodableValue("descriptors")] = descList;
                     charsList.push_back(charMap);
                 }
             }
             serviceMap[flutter::EncodableValue("characteristics")] = charsList;
             servicesList.push_back(serviceMap);
        }

        flutter::EncodableMap response;
        response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
        response[flutter::EncodableValue("services")] = servicesList;
        response[flutter::EncodableValue("success")] = flutter::EncodableValue(1);
        response[flutter::EncodableValue("error_code")] = flutter::EncodableValue(0);
        response[flutter::EncodableValue("error_string")] = flutter::EncodableValue("GATT_SUCCESS");

        co_await ui_thread_;
        channel_->InvokeMethod("OnDiscoveredServices", std::make_unique<flutter::EncodableValue>(response));
        result_ptr->Success(flutter::EncodableValue(true));
        
        co_return;

    } catch (const std::exception& e) {
        error_msg = e.what();
    } catch (...) {
        error_msg = "Unknown error";
    }

    co_await ui_thread_;
    result_ptr->Error("discoverServices", error_msg);
}

void FlutterBluePlusWindowsPlugin::OnCharacteristicValueChanged(
    std::string remote_id,
    const GattCharacteristic& sender,
    const GattValueChangedEventArgs& args) {

    [this, remote_id, sender, args]() -> winrt::fire_and_forget {
        co_await ui_thread_;

        // sender is a GattCharacteristic
        std::string service_uuid = utils::to_uuid_string(sender.Service().Uuid());
        std::string characteristic_uuid = utils::to_uuid_string(sender.Uuid());
        int32_t instance_id = static_cast<int32_t>(sender.AttributeHandle());

        std::vector<uint8_t> value = utils::to_vector(args.CharacteristicValue());

        flutter::EncodableMap response;
        response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
        response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid);
        response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid);
        response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id);
        response[flutter::EncodableValue("value")] = flutter::EncodableValue(value);
        response[flutter::EncodableValue("success")] = flutter::EncodableValue(1);
        response[flutter::EncodableValue("error_code")] = flutter::EncodableValue(0);
        response[flutter::EncodableValue("error_string")] = flutter::EncodableValue("GATT_SUCCESS");
        
        channel_->InvokeMethod("OnCharacteristicReceived", std::make_unique<flutter::EncodableValue>(response));
    }();
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::SetNotifyValueAsync(
    std::shared_ptr<flutter::EncodableMap> args_ptr) {
    
    // Move to background thread immediately
    co_await winrt::resume_background();

    flutter::EncodableMap& args = *args_ptr;
    std::string error_msg;

    try {
        std::string remote_id = utils::from_value<std::string>(&args[flutter::EncodableValue("remote_id")]);
        std::string service_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("service_uuid")]);
        std::string characteristic_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("characteristic_uuid")]);
        
        int instance_id = 0;
        auto it_instance = args.find(flutter::EncodableValue("instance_id"));
        if (it_instance != args.end()) {
            instance_id = utils::from_value<int>(&it_instance->second);
        }

        bool enable = utils::from_value<bool>(&args[flutter::EncodableValue("enable")]);
        bool force_indications = false;
        auto it_force = args.find(flutter::EncodableValue("force_indications"));
        if (it_force != args.end()) {
            force_indications = utils::from_value<bool>(&it_force->second);
        }

        BluetoothLEDevice device = nullptr;
        
        co_await ui_thread_; // Switch to UI thread to safely access connected_devices_
        {
             auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
             if (it != connected_devices_.end()) {
                 device = it->second.as<BluetoothLEDevice>();
             }
        }
        co_await winrt::resume_background(); // Switch back to background

        if (!device) {
             co_return;
        }

        // Unique key for storing subscription info
        std::string token_key = remote_id + ":" + service_uuid_str + ":" + characteristic_uuid_str + ":" + std::to_string(instance_id);

        if (enable) {
            // Already subscribed?
            if (subscribed_characteristics_.find(token_key) != subscribed_characteristics_.end()) {
                 // If we want to force re-subscribe, we might need to unsubscribe first.
                 // But typically calling setNotify again with enable=true is just a no-op or refresh.
            }

            winrt::guid serviceUuid = utils::parse_uuid(service_uuid_str);
            auto servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Cached);
            if (servicesResult.Status() != GattCommunicationStatus::Success) {
                 servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Uncached);
            }

            GattCharacteristic targetChar = nullptr;

            if (servicesResult.Status() == GattCommunicationStatus::Success) {
                for (auto service : servicesResult.Services()) {
                    // UUID checking is redundant if GetGattServicesForUuidAsync worked correctly, but safe.
                    if (utils::to_uuid_string(service.Uuid()) == service_uuid_str) {
                         winrt::guid charUuid = utils::parse_uuid(characteristic_uuid_str);
                         auto charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Cached);
                         if (charsResult.Status() != GattCommunicationStatus::Success) {
                             charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Uncached);
                         }

                         if (charsResult.Status() == GattCommunicationStatus::Success) {
                            for (auto characteristic : charsResult.Characteristics()) {
                                if (utils::to_uuid_string(characteristic.Uuid()) == characteristic_uuid_str) {
                                    if (static_cast<int32_t>(characteristic.AttributeHandle()) == instance_id) {
                                        targetChar = characteristic;
                                        break;
                                    }
                                }
                            }
                         }
                    }
                    if (targetChar) break;
                }
            }

            if (!targetChar) {
                co_return;
            }

            GattClientCharacteristicConfigurationDescriptorValue cccdValue = GattClientCharacteristicConfigurationDescriptorValue::None;
            auto props = targetChar.CharacteristicProperties();
            
            bool canNotify = (static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Notify)) != 0;
            bool canIndicate = (static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Indicate)) != 0;

            if (force_indications && canIndicate) {
                cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
            } else if (canNotify) {
                cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Notify;
            } else if (canIndicate) {
                cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
            } else {
                 co_return;
            }

            auto status = co_await targetChar.WriteClientCharacteristicConfigurationDescriptorAsync(cccdValue);

            if (status == GattCommunicationStatus::Success) {
                co_await ui_thread_;
                // Check again if subscribed to avoid race conditions
                if (subscribed_characteristics_.find(token_key) == subscribed_characteristics_.end()) {
                    // Use lambda capture for event handler to avoid sender lookup issues
                    auto token = targetChar.ValueChanged([this, remote_id](GattCharacteristic const& sender, GattValueChangedEventArgs const& args) {
                        this->OnCharacteristicValueChanged(remote_id, sender, args);
                    });
                    
                    // Store characteristic object and token to keep them alive and for unsubscription
                    subscribed_characteristics_[token_key] = { targetChar, token };
                }
            }
        } else {
             // Disable
             
             co_await ui_thread_;
             auto it = subscribed_characteristics_.find(token_key);
             GattCharacteristic targetChar = nullptr;

             if (it != subscribed_characteristics_.end()) {
                 targetChar = it->second.characteristic;
                 // Unregister event handler
                 targetChar.ValueChanged(it->second.token);
                 subscribed_characteristics_.erase(it);
             }
             
             // If we had the characteristic object, use it to disable CCCD
             if (targetChar) {
                 co_await winrt::resume_background();
                 co_await targetChar.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::None);
             } else {
                 // If characteristic wasn't found in map (unexpected), we might want to try finding it again from device
                 // to ensure notifications are disabled at the device level.
                 // For now, assuming if it's not in map, we are not subscribed.
             }
        }
        
        co_return;

    } catch (...) {
        // Ignore errors
    }
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::ReadCharacteristicAsync(
    flutter::EncodableMap args,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    
    auto result_ptr = std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>(std::move(result));
    std::string error_msg;

    try {
        std::string remote_id = utils::from_value<std::string>(&args[flutter::EncodableValue("remote_id")]);
        std::string service_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("service_uuid")]);
        std::string characteristic_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("characteristic_uuid")]);
        
        int instance_id = 0;
        auto it_instance = args.find(flutter::EncodableValue("instance_id"));
        if (it_instance != args.end()) {
            instance_id = utils::from_value<int>(&it_instance->second);
        }

        BluetoothLEDevice device = nullptr;
        {
             auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
             if (it != connected_devices_.end()) {
                 device = it->second.as<BluetoothLEDevice>();
             }
        }

        if (!device) {
             result_ptr->Error("readCharacteristic", "device is disconnected");
             co_return;
        }

        co_await winrt::resume_background();

        winrt::guid serviceUuid = utils::parse_uuid(service_uuid_str);
        auto servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Cached);
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
             servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Uncached);
        }

        GattCharacteristic targetChar = nullptr;

        if (servicesResult.Status() == GattCommunicationStatus::Success) {
            for (auto service : servicesResult.Services()) {
                if (utils::to_uuid_string(service.Uuid()) == service_uuid_str) {
                    winrt::guid charUuid = utils::parse_uuid(characteristic_uuid_str);
                    auto charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Cached);
                    if (charsResult.Status() != GattCommunicationStatus::Success) {
                        charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Uncached);
                    }

                    if (charsResult.Status() == GattCommunicationStatus::Success) {
                        for (auto characteristic : charsResult.Characteristics()) {
                            if (utils::to_uuid_string(characteristic.Uuid()) == characteristic_uuid_str) {
                                if (static_cast<int32_t>(characteristic.AttributeHandle()) == instance_id) {
                                    targetChar = characteristic;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (targetChar) break;
            }
        }

        if (!targetChar) {
            co_await ui_thread_;
            result_ptr->Error("readCharacteristic", "Characteristic not found");
            co_return;
        }

        auto readResult = co_await targetChar.ReadValueAsync(BluetoothCacheMode::Uncached);
        
        if (readResult.Status() == GattCommunicationStatus::Success) {
            std::vector<uint8_t> value = utils::to_vector(readResult.Value());
            
            flutter::EncodableMap response;
            response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid_str);
            response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid_str);
            response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id);
            response[flutter::EncodableValue("value")] = flutter::EncodableValue(value);
            response[flutter::EncodableValue("success")] = flutter::EncodableValue(1);
            
            co_await ui_thread_;
            channel_->InvokeMethod("OnCharacteristicReceived", std::make_unique<flutter::EncodableValue>(response));
            result_ptr->Success(flutter::EncodableValue(true));
        } else {
            error_msg = "Read failed: " + std::to_string((int)readResult.Status());
            if (readResult.Status() == GattCommunicationStatus::ProtocolError) {
                auto err = readResult.ProtocolError();
                if (err) {
                    error_msg += " (ATT Error: " + std::to_string(err.Value()) + ")";
                }
            }
            co_await ui_thread_;
            result_ptr->Error("readCharacteristic", error_msg);
        }
        
        co_return;

    } catch (const std::exception& e) {
        error_msg = e.what();
    } catch (...) {
        error_msg = "Unknown error";
    }

    co_await ui_thread_;
    result_ptr->Error("readCharacteristic", error_msg);
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::WriteCharacteristicAsync(
    flutter::EncodableMap args,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    
    auto result_ptr = std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>(std::move(result));
    std::string error_msg;

    try {
        std::string remote_id = utils::from_value<std::string>(&args[flutter::EncodableValue("remote_id")]);
        std::string service_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("service_uuid")]);
        std::string characteristic_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("characteristic_uuid")]);
        
        int instance_id = 0;
        auto it_instance = args.find(flutter::EncodableValue("instance_id"));
        if (it_instance != args.end()) {
            instance_id = utils::from_value<int>(&it_instance->second);
        }

        std::vector<uint8_t> value = utils::from_value<std::vector<uint8_t>>(&args[flutter::EncodableValue("value")]);
        int write_type = utils::from_value<int>(&args[flutter::EncodableValue("write_type")]);

        BluetoothLEDevice device = nullptr;
        {
             auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
             if (it != connected_devices_.end()) {
                 device = it->second.as<BluetoothLEDevice>();
             }
        }

        if (!device) {
             result_ptr->Error("writeCharacteristic", "device is disconnected");
             co_return;
        }

        co_await winrt::resume_background();

        winrt::guid serviceUuid = utils::parse_uuid(service_uuid_str);
        auto servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Cached);
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
             servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Uncached);
        }

        GattCharacteristic targetChar = nullptr;

        if (servicesResult.Status() == GattCommunicationStatus::Success) {
            for (auto service : servicesResult.Services()) {
                if (utils::to_uuid_string(service.Uuid()) == service_uuid_str) {
                    winrt::guid charUuid = utils::parse_uuid(characteristic_uuid_str);
                    auto charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Cached);
                    if (charsResult.Status() != GattCommunicationStatus::Success) {
                        charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Uncached);
                    }

                    if (charsResult.Status() == GattCommunicationStatus::Success) {
                        for (auto characteristic : charsResult.Characteristics()) {
                            if (utils::to_uuid_string(characteristic.Uuid()) == characteristic_uuid_str) {
                                if (static_cast<int32_t>(characteristic.AttributeHandle()) == instance_id) {
                                    targetChar = characteristic;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (targetChar) break;
            }
        }

        if (!targetChar) {
            co_await ui_thread_;
            result_ptr->Error("writeCharacteristic", "Characteristic not found");
            co_return;
        }

        auto writer = winrt::Windows::Storage::Streams::DataWriter();
        writer.WriteBytes(value);
        auto buffer = writer.DetachBuffer();

        GattWriteOption option = (write_type == 1) ? GattWriteOption::WriteWithoutResponse : GattWriteOption::WriteWithResponse;
        
        auto writeResult = co_await targetChar.WriteValueWithResultAsync(buffer, option);
        
        if (writeResult.Status() == GattCommunicationStatus::Success) {
            co_await ui_thread_;
            
            flutter::EncodableMap response;
            response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid_str);
            response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid_str);
            response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id);
            response[flutter::EncodableValue("value")] = flutter::EncodableValue(value);
            response[flutter::EncodableValue("success")] = flutter::EncodableValue(1);
            response[flutter::EncodableValue("error_code")] = flutter::EncodableValue(0);
            response[flutter::EncodableValue("error_string")] = flutter::EncodableValue("GATT_SUCCESS");

            channel_->InvokeMethod("OnCharacteristicWritten", std::make_unique<flutter::EncodableValue>(response));
            result_ptr->Success(flutter::EncodableValue(true));
        } else {
            error_msg = "Write failed: " + std::to_string((int)writeResult.Status());
            if (writeResult.Status() == GattCommunicationStatus::ProtocolError) {
                auto err = writeResult.ProtocolError();
                if (err) {
                    error_msg += " (ATT Error: " + std::to_string(err.Value()) + ")";
                }
            }
            
            co_await ui_thread_;
            
            flutter::EncodableMap response;
            response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid_str);
            response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid_str);
            response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id);
            response[flutter::EncodableValue("value")] = flutter::EncodableValue(value);
            response[flutter::EncodableValue("success")] = flutter::EncodableValue(0);
            
            int error_code = static_cast<int>(writeResult.Status());
            if (writeResult.Status() == GattCommunicationStatus::ProtocolError) {
                 auto err = writeResult.ProtocolError();
                 if (err) {
                     error_code = static_cast<int>(err.Value());
                 }
            }
            
            response[flutter::EncodableValue("error_code")] = flutter::EncodableValue(error_code);
            response[flutter::EncodableValue("error_string")] = flutter::EncodableValue(error_msg);

            channel_->InvokeMethod("OnCharacteristicWritten", std::make_unique<flutter::EncodableValue>(response));
            
            // 성공으로 반환해야 Dart에서 예외를 발생시키지 않고 이벤트로 에러 처리 가능
            result_ptr->Success(flutter::EncodableValue(true));
        }
        
        co_return;

    } catch (const std::exception& e) {
        error_msg = e.what();
    } catch (...) {
        error_msg = "Unknown error";
    }

    co_await ui_thread_;
    result_ptr->Error("writeCharacteristic", error_msg);
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::ReadDescriptorAsync(
    flutter::EncodableMap args,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    
    auto result_ptr = std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>(std::move(result));
    std::string error_msg;

    try {
        std::string remote_id = utils::from_value<std::string>(&args[flutter::EncodableValue("remote_id")]);
        std::string service_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("service_uuid")]);
        std::string characteristic_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("characteristic_uuid")]);
        std::string descriptor_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("descriptor_uuid")]);
        
        int instance_id = 0; 
        
        BluetoothLEDevice device = nullptr;
        {
             auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
             if (it != connected_devices_.end()) {
                 device = it->second.as<BluetoothLEDevice>();
             }
        }

        if (!device) {
             result_ptr->Error("readDescriptor", "device is disconnected");
             co_return;
        }

        co_await winrt::resume_background();

        winrt::guid serviceUuid = utils::parse_uuid(service_uuid_str);
        auto servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Cached);
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
             servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Uncached);
        }

        GattDescriptor targetDesc = nullptr;

        if (servicesResult.Status() == GattCommunicationStatus::Success) {
            for (auto service : servicesResult.Services()) {
                if (utils::to_uuid_string(service.Uuid()) == service_uuid_str) {
                    winrt::guid charUuid = utils::parse_uuid(characteristic_uuid_str);
                    auto charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Cached);
                    if (charsResult.Status() != GattCommunicationStatus::Success) {
                        charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Uncached);
                    }

                    if (charsResult.Status() == GattCommunicationStatus::Success) {
                        for (auto characteristic : charsResult.Characteristics()) {
                            if (utils::to_uuid_string(characteristic.Uuid()) == characteristic_uuid_str) {
                                if (static_cast<int32_t>(characteristic.AttributeHandle()) == instance_id) { // check instance id
                                    winrt::guid descUuid = utils::parse_uuid(descriptor_uuid_str);
                                    auto descResult = co_await characteristic.GetDescriptorsForUuidAsync(descUuid, BluetoothCacheMode::Cached);
                                    if (descResult.Status() != GattCommunicationStatus::Success) {
                                        descResult = co_await characteristic.GetDescriptorsForUuidAsync(descUuid, BluetoothCacheMode::Uncached);
                                    }
                                    
                                    if (descResult.Status() == GattCommunicationStatus::Success) {
                                        for (auto descriptor : descResult.Descriptors()) {
                                            if (utils::to_uuid_string(descriptor.Uuid()) == descriptor_uuid_str) {
                                                targetDesc = descriptor;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            if (targetDesc) break;
                        }
                    }
                }
                if (targetDesc) break;
            }
        }

        if (!targetDesc) {
            co_await ui_thread_;
            result_ptr->Error("readDescriptor", "Descriptor not found");
            co_return;
        }

        auto readResult = co_await targetDesc.ReadValueAsync(BluetoothCacheMode::Uncached);
        
        if (readResult.Status() == GattCommunicationStatus::Success) {
            std::vector<uint8_t> value = utils::to_vector(readResult.Value());
            
            flutter::EncodableMap response;
            response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid_str);
            response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid_str);
            response[flutter::EncodableValue("descriptor_uuid")] = flutter::EncodableValue(descriptor_uuid_str);
            response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id); 
            response[flutter::EncodableValue("value")] = flutter::EncodableValue(value);
            response[flutter::EncodableValue("success")] = flutter::EncodableValue(1);
            
            co_await ui_thread_;
            channel_->InvokeMethod("OnDescriptorRead", std::make_unique<flutter::EncodableValue>(response));
            result_ptr->Success(flutter::EncodableValue(true));
        } else {
            error_msg = "Read failed: " + std::to_string((int)readResult.Status());
            if (readResult.Status() == GattCommunicationStatus::ProtocolError) {
                auto err = readResult.ProtocolError();
                if (err) {
                    error_msg += " (ATT Error: " + std::to_string(err.Value()) + ")";
                }
            }
            co_await ui_thread_;
            result_ptr->Error("readDescriptor", error_msg);
        }
        
        co_return;

    } catch (const std::exception& e) {
        error_msg = e.what();
    } catch (...) {
        error_msg = "Unknown error";
    }

    co_await ui_thread_;
    result_ptr->Error("readDescriptor", error_msg);
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::WriteDescriptorAsync(
    flutter::EncodableMap args,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    
    auto result_ptr = std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>>(std::move(result));
    std::string error_msg;

    try {
        std::string remote_id = utils::from_value<std::string>(&args[flutter::EncodableValue("remote_id")]);
        std::string service_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("service_uuid")]);
        std::string characteristic_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("characteristic_uuid")]);
        std::string descriptor_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("descriptor_uuid")]);
        std::vector<uint8_t> value = utils::from_value<std::vector<uint8_t>>(&args[flutter::EncodableValue("value")]);
        
        int instance_id = 0; 

        BluetoothLEDevice device = nullptr;
        {
             auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
             if (it != connected_devices_.end()) {
                 device = it->second.as<BluetoothLEDevice>();
             }
        }

        if (!device) {
             result_ptr->Error("writeDescriptor", "device is disconnected");
             co_return;
        }

        co_await winrt::resume_background();

        winrt::guid serviceUuid = utils::parse_uuid(service_uuid_str);
        auto servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Cached);
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
             servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Uncached);
        }

        GattDescriptor targetDesc = nullptr;

        if (servicesResult.Status() == GattCommunicationStatus::Success) {
            for (auto service : servicesResult.Services()) {
                if (utils::to_uuid_string(service.Uuid()) == service_uuid_str) {
                    winrt::guid charUuid = utils::parse_uuid(characteristic_uuid_str);
                    auto charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Cached);
                    if (charsResult.Status() != GattCommunicationStatus::Success) {
                        charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Uncached);
                    }

                    if (charsResult.Status() == GattCommunicationStatus::Success) {
                        for (auto characteristic : charsResult.Characteristics()) {
                            if (utils::to_uuid_string(characteristic.Uuid()) == characteristic_uuid_str) {
                                if (static_cast<int32_t>(characteristic.AttributeHandle()) == instance_id) { // check instance id
                                    winrt::guid descUuid = utils::parse_uuid(descriptor_uuid_str);
                                    auto descResult = co_await characteristic.GetDescriptorsForUuidAsync(descUuid, BluetoothCacheMode::Cached);
                                    if (descResult.Status() != GattCommunicationStatus::Success) {
                                        descResult = co_await characteristic.GetDescriptorsForUuidAsync(descUuid, BluetoothCacheMode::Uncached);
                                    }
                                    
                                    if (descResult.Status() == GattCommunicationStatus::Success) {
                                        for (auto descriptor : descResult.Descriptors()) {
                                            if (utils::to_uuid_string(descriptor.Uuid()) == descriptor_uuid_str) {
                                                targetDesc = descriptor;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            if (targetDesc) break;
                        }
                    }
                }
                if (targetDesc) break;
            }
        }

        if (!targetDesc) {
            co_await ui_thread_;
            result_ptr->Error("writeDescriptor", "Descriptor not found");
            co_return;
        }

        auto writer = winrt::Windows::Storage::Streams::DataWriter();
        writer.WriteBytes(value);
        auto buffer = writer.DetachBuffer();
        
        auto writeResult = co_await targetDesc.WriteValueWithResultAsync(buffer);
        
        if (writeResult.Status() == GattCommunicationStatus::Success) {
            co_await ui_thread_;
            
            flutter::EncodableMap response;
            response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid_str);
            response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid_str);
            response[flutter::EncodableValue("descriptor_uuid")] = flutter::EncodableValue(descriptor_uuid_str);
            response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id);
            response[flutter::EncodableValue("value")] = flutter::EncodableValue(value);
            response[flutter::EncodableValue("success")] = flutter::EncodableValue(1);
            response[flutter::EncodableValue("error_code")] = flutter::EncodableValue(0);
            response[flutter::EncodableValue("error_string")] = flutter::EncodableValue("GATT_SUCCESS");

            channel_->InvokeMethod("OnDescriptorWritten", std::make_unique<flutter::EncodableValue>(response));
            result_ptr->Success(flutter::EncodableValue(true));
        } else {
            error_msg = "Write failed: " + std::to_string((int)writeResult.Status());
            if (writeResult.Status() == GattCommunicationStatus::ProtocolError) {
                auto err = writeResult.ProtocolError();
                if (err) {
                    error_msg += " (ATT Error: " + std::to_string(err.Value()) + ")";
                }
            }
            co_await ui_thread_;
            result_ptr->Error("writeDescriptor", error_msg);
        }
        
        co_return;

    } catch (const std::exception& e) {
        error_msg = e.what();
    } catch (...) {
        error_msg = "Unknown error";
    }

    co_await ui_thread_;
    result_ptr->Error("writeDescriptor", error_msg);
}

void FlutterBluePlusWindowsPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const auto& method = method_call.method_name();

    if (method == "flutterRestart") {
        watcher_.Stop();

        int count = static_cast<int>(connected_devices_.size());

        for (const auto& pair : connected_devices_) {
            auto device = pair.second.as<BluetoothLEDevice>();
            if (device) {
                device.Close(); 
            }
        }
        
        connected_devices_.clear();
        currently_connecting_devices_.clear();
        rssi_cache_.clear();
        scan_results_cache_.clear();
        subscribed_characteristics_.clear();

        result->Success(flutter::EncodableValue(count));
        return;
    }

    if (method == "setLogLevel") {
        result->Success(flutter::EncodableValue(true));
        return;
    }

    if (method == "startScan") {
        scan_results_cache_.clear();
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
        GetSystemDevicesAsync(std::move(result));
        return;
    }

    if (method == "getAdapterState") {
        GetAdapterStateAsync(std::move(result));
        return;
    }

    if (method == "connect") {
        const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
        std::string remote_id;
        if (args) {
            auto it = args->find(flutter::EncodableValue("remote_id"));
            if (it != args->end()) {
                remote_id = utils::from_value<std::string>(&it->second);
            }
        }
        ConnectAsync(remote_id, std::move(result));
        return;
    }

    if (method == "disconnect") {
        const auto* remote_id_val_ptr = std::get_if<std::string>(method_call.arguments());
        if (remote_id_val_ptr) {
            std::string remote_id = *remote_id_val_ptr;
            auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
            if (it != connected_devices_.end()) {
                auto device = it->second.as<BluetoothLEDevice>();
                if (device) {
                     device.Close(); 
                }
                
                flutter::EncodableMap connection_state;
                connection_state[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(0); 
                channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));

            } else {
                 auto it_connecting = std::find_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                    [&](const auto& pair) { return pair.first == remote_id; });
                 if (it_connecting != currently_connecting_devices_.end()) {
                    auto device_connecting = it_connecting->second.as<BluetoothLEDevice>();
                    if (device_connecting) {
                        device_connecting.Close(); 
                    }
                 }
            }
        }
        result->Success(flutter::EncodableValue(true));
        return;
    }
    
    if (method_call.method_name() == "readRssi") { 
        const auto* remote_id_arg = std::get_if<std::string>(method_call.arguments());
        if (!remote_id_arg) {
            result->Error("InvalidArgument", "Expected a string remote_id argument.");
            return; 
        }
        const std::string remote_id = *remote_id_arg;

        flutter::EncodableMap rssi_update;
        rssi_update[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);

        auto rssi_it = rssi_cache_.find(remote_id);
        if (rssi_it != rssi_cache_.end()) {
            rssi_update[flutter::EncodableValue("rssi")] = flutter::EncodableValue(static_cast<int32_t>(rssi_it->second));
            rssi_update[flutter::EncodableValue("success")] = flutter::EncodableValue(true);
            rssi_update[flutter::EncodableValue("error_code")] = flutter::EncodableValue(0); 
            rssi_update[flutter::EncodableValue("error_string")] = flutter::EncodableValue(""); 
        } else {
            rssi_update[flutter::EncodableValue("rssi")] = flutter::EncodableValue(0); 
            rssi_update[flutter::EncodableValue("success")] = flutter::EncodableValue(false);
            rssi_update[flutter::EncodableValue("error_code")] = flutter::EncodableValue(1); 
            rssi_update[flutter::EncodableValue("error_string")] = flutter::EncodableValue("RSSI not available in cache.");
        }
        channel_->InvokeMethod("OnReadRssi", std::make_unique<flutter::EncodableValue>(rssi_update));
        result->Success(flutter::EncodableValue(true));
        return; 
    }

    if (method == "discoverServices") {
        const auto* remote_id_val = std::get_if<std::string>(method_call.arguments());
        if (remote_id_val) {
             DiscoverServicesAsync(*remote_id_val, std::move(result));
        } else {
            result->Error("discoverServices", "Invalid arguments");
        }
        return;
    }
    
    if (method == "setNotifyValue") {
        const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
        if (args) {
            // Create a shared_ptr copy of args to pass to async function safely
            auto args_ptr = std::make_shared<flutter::EncodableMap>(*args);
            
            // Return success immediately to avoid timeout
            result->Success(flutter::EncodableValue(true)); 
            
            SetNotifyValueAsync(args_ptr);
        } else {
             result->Error("setNotifyValue", "Invalid arguments");
        }
        return;
    }

    if (method == "readCharacteristic") {
        const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
        if (args) {
            ReadCharacteristicAsync(*args, std::move(result));
        } else {
             result->Error("readCharacteristic", "Invalid arguments");
        }
        return;
    }

    if (method == "writeCharacteristic") {
        const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
        if (args) {
            WriteCharacteristicAsync(*args, std::move(result));
        } else {
             result->Error("writeCharacteristic", "Invalid arguments");
        }
        return;
    }

    if (method == "readDescriptor") {
        const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
        if (args) {
            ReadDescriptorAsync(*args, std::move(result));
        } else {
             result->Error("readDescriptor", "Invalid arguments");
        }
        return;
    }

    if (method == "writeDescriptor") {
        const auto* args = std::get_if<flutter::EncodableMap>(method_call.arguments());
        if (args) {
            WriteDescriptorAsync(*args, std::move(result));
        } else {
             result->Error("writeDescriptor", "Invalid arguments");
        }
        return;
    }

    if (method == "connectedCount") {
        result->Success(flutter::EncodableValue(static_cast<int>(connected_devices_.size())));
        return;
    }

    // TODO: Implement other methods like requestMtu, etc.

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