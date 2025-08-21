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
    if (channel_) {
        std::string remote_id = uint64_to_mac_string(args.BluetoothAddress());
        
        rssi_cache_[remote_id] = static_cast<int32_t>(args.RawSignalStrengthInDBm());

        flutter::EncodableMap map;
        map[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);

        auto advertisement = args.Advertisement();

        // adv_name & platform_name
        std::string localNameStr = utils::to_string(advertisement.LocalName());
        map[flutter::EncodableValue("adv_name")] = flutter::EncodableValue(localNameStr);
        map[flutter::EncodableValue("platform_name")] = flutter::EncodableValue(localNameStr);

        map[flutter::EncodableValue("rssi")] =
            flutter::EncodableValue(static_cast<int32_t>(args.RawSignalStrengthInDBm()));

        // connectable
        auto adType = args.AdvertisementType();
        bool connectable_bool = (adType == BluetoothLEAdvertisementType::ConnectableUndirected) ||
                                (adType == BluetoothLEAdvertisementType::ConnectableDirected);
        map[flutter::EncodableValue("connectable")] = flutter::EncodableValue(connectable_bool ? 1 : 0);

        // tx_power_level
        if (args.TransmitPowerLevelInDBm() != nullptr) {
            map[flutter::EncodableValue("tx_power_level")] =
                flutter::EncodableValue(static_cast<int32_t>(args.TransmitPowerLevelInDBm().Value()));
        }

        // appearance
        for (const auto& section : advertisement.DataSections()) {
            if (section.DataType() == 0x19) { // Appearance
                auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(section.Data());
                uint16_t appearance_value;
                appearance_value = reader.ReadUInt16();
                map[flutter::EncodableValue("appearance")] = flutter::EncodableValue(static_cast<int32_t>(appearance_value));
                break;
            }
        }

        // manufacturer_data
        if (advertisement.ManufacturerData().Size() > 0) {
            flutter::EncodableMap msd_map;
            for (const auto& msd : advertisement.ManufacturerData()) {
                msd_map[flutter::EncodableValue(static_cast<int64_t>(msd.CompanyId()))] = 
                    flutter::EncodableValue(utils::to_vector(msd.Data()));
            }
            map[flutter::EncodableValue("manufacturer_data")] = msd_map;
        }

        // service_data
        flutter::EncodableMap service_data_map;
        for (const auto& section : advertisement.GetSectionsByType(0x16)) {
             auto buffer = section.Data();
             if (buffer.Length() >= 2) {
                 auto all_data = utils::to_vector(buffer);
                 uint16_t uuid16 = (all_data[1] << 8) | all_data[0];
                 std::stringstream ss;
                 ss << std::hex << std::setfill('0') << std::setw(4) << uuid16;
                 std::string uuid_str = "0000" + ss.str() + "-0000-1000-8000-00805f9b34fb";
                 std::vector<uint8_t> data_vec(all_data.begin() + 2, all_data.end());
                 service_data_map[flutter::EncodableValue(uuid_str)] = flutter::EncodableValue(data_vec);
             }
        }
        if (!service_data_map.empty()) {
            map[flutter::EncodableValue("service_data")] = service_data_map;
        }

        // service_uuids
        if (advertisement.ServiceUuids().Size() > 0) {
            flutter::EncodableList service_uuids_list;
            for (const auto& uuid : advertisement.ServiceUuids()) {
                 service_uuids_list.push_back(flutter::EncodableValue(utils::to_uuid_string(uuid)));
            }
            map[flutter::EncodableValue("service_uuids")] = service_uuids_list;
        }

        flutter::EncodableMap response;
        response[flutter::EncodableValue("advertisements")] = flutter::EncodableList{ flutter::EncodableValue(map) };
        channel_->InvokeMethod("OnScanResponse", std::make_unique<flutter::EncodableValue>(response));
    }
}

void FlutterBluePlusWindowsPlugin::OnAdvertisementStopped(
    const BluetoothLEAdvertisementWatcher&,
    const BluetoothLEAdvertisementWatcherStoppedEventArgs&) {
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::GetSystemDevicesAsync(std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
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

                auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                    [&](const auto& pair) { return pair.first == remote_id; });

                bool is_connected = (it != connected_devices_.end());

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
        result->Success(flutter::EncodableValue(response));
    }
    catch (const hresult_error& e) {
        result->Error("getSystemDevices", utils::to_string(e.message()));
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
        result->Success(flutter::EncodableValue(response));
    }
    catch (const hresult_error& e) {
        result->Error("getAdapterState", utils::to_string(e.message()));
    }
    catch (const std::exception& e) {
        result->Error("getAdapterState", e.what());
    }
    co_return;
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::ConnectAsync(
    std::string remote_id,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    try {
        uint64_t bluetoothAddress = utils::mac_to_uint64(remote_id);
        auto device = co_await BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);

        if (device) {
            auto it = std::find_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
            if (it == currently_connecting_devices_.end()) {
                 currently_connecting_devices_.emplace_back(remote_id, device);
            }

            device.ConnectionStatusChanged({ this, &FlutterBluePlusWindowsPlugin::OnConnectionStatusChanged });
            
            // Trigger connection by getting GATT services
            auto gatt_result = co_await device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
            
            if (gatt_result.Status() == GattCommunicationStatus::Success) {
                // ConnectionStatusChanged will handle moving to connected_devices_
                 result->Success(flutter::EncodableValue(true));
            } else {
                 currently_connecting_devices_.erase(
                    std::remove_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                        [&](const auto& pair) { return pair.first == remote_id; }),
                    currently_connecting_devices_.end());
                result->Error("connect", "Failed to connect: " + std::to_string((int)gatt_result.Status()));
            }
        }
        else {
            result->Error("connect", "Device not found.");
        }
    }
    catch (const hresult_error& e) {
        result->Error("connect", utils::to_string(e.message()));
    }
    co_return;
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
    
    std::string remote_id = uint64_to_mac_string(device.BluetoothAddress());
    flutter::EncodableMap connection_state;
    connection_state[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);

    if (device.ConnectionStatus() == BluetoothConnectionStatus::Connected) {

        auto it_connecting = std::find_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
            [&](const auto& pair) { return pair.first == remote_id; });

        if(it_connecting != currently_connecting_devices_.end()) {
             connected_devices_.emplace_back(remote_id, device);
             currently_connecting_devices_.erase(it_connecting);
        }
        
        connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(1); // connected
        channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));

    } else if (device.ConnectionStatus() == BluetoothConnectionStatus::Disconnected) {
       
        auto it_connected = std::find_if(connected_devices_.begin(), connected_devices_.end(),
            [&](const auto& pair) { return pair.first == remote_id; });

        if (it_connected != connected_devices_.end()) {
            connected_devices_.erase(it_connected);
        }

        connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(0); // disconnected
        channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));
    }
}

winrt::fire_and_forget FlutterBluePlusWindowsPlugin::DiscoverServicesAsync(
    std::string remote_id,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    try {
        auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
            [&](const auto& pair) { return pair.first == remote_id; });
        if (it == connected_devices_.end()) {
            result->Error("discoverServices", "Device not found");
            co_return;
        }
        auto device = it->second.as<BluetoothLEDevice>();

        auto gatt_result = co_await device.GetGattServicesAsync(BluetoothCacheMode::Uncached);

        if (gatt_result.Status() != GattCommunicationStatus::Success) {
            result->Error("discoverServices", "Failed to discover services: " + std::to_string((int)gatt_result.Status()));
            co_return;
        }

        flutter::EncodableList services_list;
        for (const auto& service : gatt_result.Services()) {
            flutter::EncodableMap service_map;
            service_map[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            service_map[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(utils::to_uuid_string(service.Uuid()));
            
            auto char_result = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
            if (char_result.Status() == GattCommunicationStatus::Success) {
                flutter::EncodableList characteristics_list;
                for (const auto& characteristic : char_result.Characteristics()) {
                    flutter::EncodableMap characteristic_map;
                    characteristic_map[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                    characteristic_map[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(utils::to_uuid_string(service.Uuid()));
                    characteristic_map[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(utils::to_uuid_string(characteristic.Uuid()));
                    
                    auto props = characteristic.CharacteristicProperties();
                    flutter::EncodableMap properties_map;
                    properties_map[flutter::EncodableValue("broadcast")] = flutter::EncodableValue((props & GattCharacteristicProperties::Broadcast) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("read")] = flutter::EncodableValue((props & GattCharacteristicProperties::Read) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("write_without_response")] = flutter::EncodableValue((props & GattCharacteristicProperties::WriteWithoutResponse) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("write")] = flutter::EncodableValue((props & GattCharacteristicProperties::Write) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("notify")] = flutter::EncodableValue((props & GattCharacteristicProperties::Notify) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("indicate")] = flutter::EncodableValue((props & GattCharacteristicProperties::Indicate) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("authenticated_signed_writes")] = flutter::EncodableValue((props & GattCharacteristicProperties::AuthenticatedSignedWrites) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("extended_properties")] = flutter::EncodableValue((props & GattCharacteristicProperties::ExtendedProperties) != GattCharacteristicProperties::None);
                    properties_map[flutter::EncodableValue("notify_encryption_required")] = flutter::EncodableValue(false);
                    properties_map[flutter::EncodableValue("indicate_encryption_required")] = flutter::EncodableValue(false);
                    characteristic_map[flutter::EncodableValue("properties")] = properties_map;

                    auto desc_result = co_await characteristic.GetDescriptorsAsync(BluetoothCacheMode::Uncached);
                    if (desc_result.Status() == GattCommunicationStatus::Success) {
                        flutter::EncodableList descriptors_list;
                        for (const auto& descriptor : desc_result.Descriptors()) {
                             flutter::EncodableMap descriptor_map;
                             descriptor_map[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                             descriptor_map[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(utils::to_uuid_string(service.Uuid()));
                             descriptor_map[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(utils::to_uuid_string(characteristic.Uuid()));
                             descriptor_map[flutter::EncodableValue("descriptor_uuid")] = flutter::EncodableValue(utils::to_uuid_string(descriptor.Uuid()));
                             descriptors_list.push_back(descriptor_map);
                        }
                        characteristic_map[flutter::EncodableValue("descriptors")] = descriptors_list;
                    }
                    characteristics_list.push_back(characteristic_map);
                }
                service_map[flutter::EncodableValue("characteristics")] = characteristics_list;
            }
            services_list.push_back(service_map);
        }

        flutter::EncodableMap response;
        response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
        response[flutter::EncodableValue("services")] = services_list;
        response[flutter::EncodableValue("success")] = flutter::EncodableValue(true);
        channel_->InvokeMethod("OnDiscoveredServices", std::make_unique<flutter::EncodableValue>(response));

        result->Success(flutter::EncodableValue(true));

    } catch (const hresult_error& e) {
        result->Error("discoverServices", utils::to_string(e.message()));
    }
    co_return;
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
        result->Success(flutter::EncodableValue(count));
        return;
    }

    if (method == "setLogLevel") {
        // Logging is not implemented in the Windows plugin, but we need to handle the call
        result->Success(flutter::EncodableValue(true));
        return;
    }

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
        const auto* remote_id_val = std::get_if<std::string>(method_call.arguments());
        if (remote_id_val) {
            auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == *remote_id_val; });
            if (it != connected_devices_.end()) {
                auto device = it->second.as<BluetoothLEDevice>();
                device.Close(); // This will trigger OnConnectionStatusChanged
            }
        }
        result->Success(flutter::EncodableValue(true));
        return;
    }

    if (method == "readRssi") {
        const auto* remote_id_val = std::get_if<std::string>(method_call.arguments());
        if (remote_id_val) {
            auto it = rssi_cache_.find(*remote_id_val);
            if (it != rssi_cache_.end()) {
                flutter::EncodableMap rssi_result;
                rssi_result[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(*remote_id_val);
                rssi_result[flutter::EncodableValue("rssi")] = flutter::EncodableValue(it->second);
                channel_->InvokeMethod("OnReadRssi", std::make_unique<flutter::EncodableValue>(rssi_result));
                 result->Success(flutter::EncodableValue(true));
            } else {
                 result->Error("readRssi", "RSSI not available");
            }
        } else {
            result->Error("readRssi", "Invalid arguments");
        }
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

    if (method == "connectedCount") {
        result->Success(flutter::EncodableValue(static_cast<int>(connected_devices_.size())));
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
