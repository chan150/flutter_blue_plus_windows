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
#include <cstdarg>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Radios;

namespace flutter_blue_plus_windows {

// Debug Logging Helper
void Log(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    std::string msg = "[FBP-Win] ";
    msg += buffer;
    msg += "\n";
    OutputDebugStringA(msg.c_str());
}

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
    if (full_uuid.length() == 36 &&
        full_uuid.substr(0, 4) == "0000" &&
        full_uuid.substr(8) == "-0000-1000-8000-00805f9b34fb") {
        return full_uuid.substr(4, 4); // Return short form like "180d"
    }
    return full_uuid;
}

// Helper to parse UUID string to winrt::guid
winrt::guid parse_uuid(const std::string& uuid_str) {
    std::string full_uuid = uuid_str;
    if (full_uuid.length() == 4) {
        full_uuid = "0000" + full_uuid + "-0000-1000-8000-00805f9b34fb";
    }
    else if (full_uuid.length() == 8) {
        full_uuid = full_uuid + "-0000-1000-8000-00805f9b34fb";
    }
    
    // Add braces if not present for GUID parsing
    if (full_uuid.length() > 0 && full_uuid.front() != '{') {
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

struct CharacteristicAndDescriptor {
    GattCharacteristic characteristic;
    GattDescriptor descriptor;
};

// Forward Declarations
winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> FindCharacteristicInServiceAsync(
    GattDeviceService service,
    int instance_id,
    BluetoothCacheMode cacheMode);

winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> GetCharacteristicByHandleAsync(
    BluetoothLEDevice device,
    int instance_id);

winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> GetCharacteristicAsync(
    BluetoothLEDevice device,
    std::string service_uuid_str,
    std::string characteristic_uuid_str,
    std::string primary_service_uuid_str,
    int instance_id);

winrt::Windows::Foundation::IAsyncOperation<GattDescriptor> GetDescriptorAsync(
    GattCharacteristic characteristic,
    std::string descriptor_uuid_str);

// New Helper: Find Characteristic and Descriptor by Descriptor Handle
winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> FindCharacteristicByDescriptorHandleInServiceAsync(
    GattDeviceService service,
    int descriptor_handle,
    BluetoothCacheMode cacheMode);

winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> GetCharacteristicByDescriptorHandleAsync(
    BluetoothLEDevice device,
    int descriptor_handle);

// Helper function to populate characteristics list (avoids IAsyncOperation template issues with std::vector)
winrt::Windows::Foundation::IAsyncAction PopulateCharacteristicsAsync(
    GattDeviceService service,
    std::string remote_id,
    std::string primaryServiceUuid,
    std::shared_ptr<flutter::EncodableList> outList)
{
    auto charsResult = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);

    if (charsResult.Status() == GattCommunicationStatus::Success) {
        for (auto characteristic : charsResult.Characteristics()) {
            flutter::EncodableMap charMap;
            std::string serviceUuid = utils::to_uuid_string(service.Uuid());
            std::string charUuid = utils::to_uuid_string(characteristic.Uuid());

            charMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            if (!primaryServiceUuid.empty()) {
                charMap[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(primaryServiceUuid);
            }
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

            // Do NOT read descriptors here to avoid performance hit and to keep logic simple.
            // Just discovery. The `descriptors` list will be populated with basic info.
            auto descResult = co_await characteristic.GetDescriptorsAsync(BluetoothCacheMode::Uncached);
            flutter::EncodableList descList;
            if (descResult.Status() == GattCommunicationStatus::Success) {
                for (auto descriptor : descResult.Descriptors()) {
                    flutter::EncodableMap descMap;
                    descMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                    if (!primaryServiceUuid.empty()) {
                        descMap[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(primaryServiceUuid);
                    }
                    descMap[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(serviceUuid);
                    descMap[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(charUuid);
                    descMap[flutter::EncodableValue("descriptor_uuid")] = flutter::EncodableValue(utils::to_uuid_string(descriptor.Uuid()));
                    descMap[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(static_cast<int32_t>(descriptor.AttributeHandle()));
                    
                    descList.push_back(descMap);
                }
            }
            charMap[flutter::EncodableValue("descriptors")] = descList;
            outList->push_back(charMap);
        }
    }
    co_return;
}

// Recursive helper function to find a characteristic by handle
winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> FindCharacteristicInServiceAsync(
    GattDeviceService service,
    int instance_id,
    BluetoothCacheMode cacheMode)
{
    // 1. Check characteristics in this service
    auto charsResult = co_await service.GetCharacteristicsAsync(cacheMode);
    if (charsResult.Status() == GattCommunicationStatus::Success) {
        for (auto c : charsResult.Characteristics()) {
            if (instance_id != 0 && static_cast<int32_t>(c.AttributeHandle()) == instance_id) {
                co_return c;
            }
        }
    }

    // 2. Check included services
    auto includedResult = co_await service.GetIncludedServicesAsync(cacheMode);
    if (includedResult.Status() == GattCommunicationStatus::Success) {
        for (auto includedService : includedResult.Services()) {
            auto c = co_await FindCharacteristicInServiceAsync(includedService, instance_id, cacheMode);
            if (c) {
                co_return c;
            }
        }
    }

    co_return nullptr;
}

// New Recursive helper: Find Characteristic by looking for a child Descriptor with specific handle
winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> FindCharacteristicByDescriptorHandleInServiceAsync(
    GattDeviceService service,
    int descriptor_handle,
    BluetoothCacheMode cacheMode)
{
    // 1. Check characteristics in this service
    auto charsResult = co_await service.GetCharacteristicsAsync(cacheMode);
    if (charsResult.Status() == GattCommunicationStatus::Success) {
        for (auto c : charsResult.Characteristics()) {
             // Check descriptors of this characteristic
             auto descResult = co_await c.GetDescriptorsAsync(cacheMode);
             if (descResult.Status() == GattCommunicationStatus::Success) {
                 for (auto d : descResult.Descriptors()) {
                     if (descriptor_handle != 0 && static_cast<int32_t>(d.AttributeHandle()) == descriptor_handle) {
                         co_return c; // Found the parent characteristic!
                     }
                 }
             }
        }
    }

    // 2. Check included services
    auto includedResult = co_await service.GetIncludedServicesAsync(cacheMode);
    if (includedResult.Status() == GattCommunicationStatus::Success) {
        for (auto includedService : includedResult.Services()) {
            auto c = co_await FindCharacteristicByDescriptorHandleInServiceAsync(includedService, descriptor_handle, cacheMode);
            if (c) {
                co_return c;
            }
        }
    }

    co_return nullptr;
}

winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> GetCharacteristicByHandleAsync(
    BluetoothLEDevice device,
    int instance_id) 
{
    Log("GetCharacteristicByHandleAsync: looking for handle %d", instance_id);

    // Try Cached
    auto servicesResult = co_await device.GetGattServicesAsync(BluetoothCacheMode::Cached);
    if (servicesResult.Status() == GattCommunicationStatus::Success) {
        for (auto service : servicesResult.Services()) {
            auto c = co_await FindCharacteristicInServiceAsync(service, instance_id, BluetoothCacheMode::Cached);
            if (c) co_return c;
        }
    }

    // Try Uncached
    servicesResult = co_await device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
    if (servicesResult.Status() == GattCommunicationStatus::Success) {
        for (auto service : servicesResult.Services()) {
            auto c = co_await FindCharacteristicInServiceAsync(service, instance_id, BluetoothCacheMode::Uncached);
            if (c) co_return c;
        }
    }
    
    Log("GetCharacteristicByHandleAsync: not found");
    co_return nullptr;
}

winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> GetCharacteristicByDescriptorHandleAsync(
    BluetoothLEDevice device,
    int descriptor_handle) 
{
    Log("GetCharacteristicByDescriptorHandleAsync: looking for desc handle %d", descriptor_handle);

    // Try Cached
    auto servicesResult = co_await device.GetGattServicesAsync(BluetoothCacheMode::Cached);
    if (servicesResult.Status() == GattCommunicationStatus::Success) {
        for (auto service : servicesResult.Services()) {
            auto c = co_await FindCharacteristicByDescriptorHandleInServiceAsync(service, descriptor_handle, BluetoothCacheMode::Cached);
            if (c) co_return c;
        }
    }

    // Try Uncached
    servicesResult = co_await device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
    if (servicesResult.Status() == GattCommunicationStatus::Success) {
        for (auto service : servicesResult.Services()) {
            auto c = co_await FindCharacteristicByDescriptorHandleInServiceAsync(service, descriptor_handle, BluetoothCacheMode::Uncached);
            if (c) co_return c;
        }
    }
    
    Log("GetCharacteristicByDescriptorHandleAsync: not found");
    co_return nullptr;
}

// Unified function to get a characteristic by handle (preferred) or UUIDs
winrt::Windows::Foundation::IAsyncOperation<GattCharacteristic> GetCharacteristicAsync(
    BluetoothLEDevice device,
    std::string service_uuid_str,
    std::string characteristic_uuid_str,
    std::string primary_service_uuid_str,
    int instance_id)
{
    GattCharacteristic targetChar = nullptr;
    
    Log("GetCharacteristicAsync: svc=%s, chr=%s, primary=%s, handle=%d", service_uuid_str.c_str(), characteristic_uuid_str.c_str(), primary_service_uuid_str.c_str(), instance_id);

    if (!primary_service_uuid_str.empty()) {
        // Secondary Service Case
        winrt::guid primaryUuid = utils::parse_uuid(primary_service_uuid_str);
        GattDeviceServicesResult primaryResult = nullptr;

        // Try Cached
        primaryResult = co_await device.GetGattServicesForUuidAsync(primaryUuid, BluetoothCacheMode::Cached);
        if (primaryResult.Status() != GattCommunicationStatus::Success) {
             primaryResult = co_await device.GetGattServicesForUuidAsync(primaryUuid, BluetoothCacheMode::Uncached);
        }

        if (primaryResult.Status() == GattCommunicationStatus::Success) {
            for (auto primaryService : primaryResult.Services()) {
                winrt::guid serviceUuid = utils::parse_uuid(service_uuid_str);
                GattDeviceServicesResult includedResult = nullptr;

                includedResult = co_await primaryService.GetIncludedServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Cached);
                if (includedResult.Status() != GattCommunicationStatus::Success) {
                     includedResult = co_await primaryService.GetIncludedServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Uncached);
                }

                if (includedResult.Status() == GattCommunicationStatus::Success) {
                    for (auto service : includedResult.Services()) {
                        winrt::guid charUuid = utils::parse_uuid(characteristic_uuid_str);
                        GattCharacteristicsResult charsResult = nullptr;

                        charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Cached);
                        if (charsResult.Status() != GattCommunicationStatus::Success) {
                             charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Uncached);
                        }

                        if (charsResult.Status() == GattCommunicationStatus::Success) {
                            for (auto characteristic : charsResult.Characteristics()) {
                                if (instance_id == 0 || static_cast<int32_t>(characteristic.AttributeHandle()) == instance_id) {
                                    targetChar = characteristic;
                                    break;
                                }
                            }
                        }
                        if (targetChar) break;
                    }
                }
                if (targetChar) break;
            }
        }
    } else {
        // Primary Service Case
        winrt::guid serviceUuid = utils::parse_uuid(service_uuid_str);
        GattDeviceServicesResult servicesResult = nullptr;

        // Try Cached
        servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Cached);
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
             servicesResult = co_await device.GetGattServicesForUuidAsync(serviceUuid, BluetoothCacheMode::Uncached);
        }

        if (servicesResult.Status() == GattCommunicationStatus::Success) {
            for (auto service : servicesResult.Services()) {
                // Check UUID match again to be safe, sometimes Windows returns loose matches
                if (utils::to_uuid_string(service.Uuid()) == service_uuid_str) {
                     winrt::guid charUuid = utils::parse_uuid(characteristic_uuid_str);
                     auto charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Cached);
                     if (charsResult.Status() != GattCommunicationStatus::Success) {
                          charsResult = co_await service.GetCharacteristicsForUuidAsync(charUuid, BluetoothCacheMode::Uncached);
                     }
                     if (charsResult.Status() == GattCommunicationStatus::Success && charsResult.Characteristics().Size() > 0) {
                          targetChar = charsResult.Characteristics().GetAt(0);
                          for (auto c : charsResult.Characteristics()) {
                                if(instance_id != 0 && static_cast<int32_t>(c.AttributeHandle()) == instance_id) {
                                    targetChar = c;
                                    break;
                                }
                          }
                     }
                }
                if (targetChar) break;
            }
        }
    }

    if (targetChar) {
        Log("GetCharacteristicAsync: Found by UUID");
    } else {
        Log("GetCharacteristicAsync: Not found");
    }

    co_return targetChar;
}

// Helper to find descriptor within a characteristic
winrt::Windows::Foundation::IAsyncOperation<GattDescriptor> GetDescriptorAsync(
    GattCharacteristic characteristic,
    std::string descriptor_uuid_str)
{
    if (!characteristic) co_return nullptr;

    winrt::guid descUuid = utils::parse_uuid(descriptor_uuid_str);
    GattDescriptor targetDesc = nullptr;

    // Try Cached
    auto descResult = co_await characteristic.GetDescriptorsForUuidAsync(descUuid, BluetoothCacheMode::Cached);
    if (descResult.Status() == GattCommunicationStatus::Success) {
        for (auto descriptor : descResult.Descriptors()) {
            if (utils::to_uuid_string(descriptor.Uuid()) == descriptor_uuid_str) {
                targetDesc = descriptor;
                break;
            }
        }
    }

    if (targetDesc) co_return targetDesc;

    // Try Uncached
    descResult = co_await characteristic.GetDescriptorsForUuidAsync(descUuid, BluetoothCacheMode::Uncached);
    if (descResult.Status() == GattCommunicationStatus::Success) {
        for (auto descriptor : descResult.Descriptors()) {
            if (utils::to_uuid_string(descriptor.Uuid()) == descriptor_uuid_str) {
                targetDesc = descriptor;
                break;
            }
        }
    }

    co_return targetDesc;
}


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
            // First try cached
            auto gatt_result = co_await device.GetGattServicesAsync(BluetoothCacheMode::Cached);
            
            // If cached failed OR cached success but not connected -> try uncached to force connection
            if (gatt_result.Status() != GattCommunicationStatus::Success || 
                device.ConnectionStatus() != BluetoothConnectionStatus::Connected) {
                
                Log("ConnectAsync: Trying Uncached connection...");
                // Retry with Uncached to force physical connection
                gatt_result = co_await device.GetGattServicesAsync(BluetoothCacheMode::Uncached);
            }

            if (gatt_result.Status() == GattCommunicationStatus::Success && device.ConnectionStatus() == BluetoothConnectionStatus::Connected) {
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
                error_msg = "Failed to connect: " + std::to_string((int)gatt_result.Status()) + " or Device not connected.";
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

             auto charsList = std::make_shared<flutter::EncodableList>();
             co_await PopulateCharacteristicsAsync(service, remote_id, "", charsList);
             serviceMap[flutter::EncodableValue("characteristics")] = *charsList;

             servicesList.push_back(serviceMap);
             
             // Process Included Services
             auto includedResult = co_await service.GetIncludedServicesAsync(BluetoothCacheMode::Uncached);
             if (includedResult.Status() == GattCommunicationStatus::Success) {
                 for (auto includedService : includedResult.Services()) {
                     flutter::EncodableMap includedMap;
                     std::string includedUuid = utils::to_uuid_string(includedService.Uuid());
                     includedMap[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
                     includedMap[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(includedUuid);
                     includedMap[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(serviceUuid);
                     
                     auto includedCharsList = std::make_shared<flutter::EncodableList>();
                     co_await PopulateCharacteristicsAsync(includedService, remote_id, serviceUuid, includedCharsList);
                     includedMap[flutter::EncodableValue("characteristics")] = *includedCharsList;
                     
                     servicesList.push_back(includedMap);
                 }
             }
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
    
    // Variables for Event
    std::string remote_id;
    std::string service_uuid_str;
    std::string characteristic_uuid_str;
    std::string primary_service_uuid_str;
    int instance_id = 0;
    bool success_event = false;
    std::string error_string = "Unknown Error";
    std::vector<uint8_t> return_value = {0x00, 0x00};
    GattCharacteristic targetChar = nullptr;
    bool enable = false;
    bool force_indications = false;

    try {
        remote_id = utils::from_value<std::string>(&args[flutter::EncodableValue("remote_id")]);
        service_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("service_uuid")]);
        characteristic_uuid_str = utils::from_value<std::string>(&args[flutter::EncodableValue("characteristic_uuid")]);
        
        auto it_primary = args.find(flutter::EncodableValue("primary_service_uuid"));
        if (it_primary != args.end()) {
            primary_service_uuid_str = utils::from_value<std::string>(&it_primary->second);
        }

        auto it_instance = args.find(flutter::EncodableValue("instance_id"));
        if (it_instance != args.end()) {
            instance_id = utils::from_value<int>(&it_instance->second);
        }

        enable = utils::from_value<bool>(&args[flutter::EncodableValue("enable")]);
        auto it_force = args.find(flutter::EncodableValue("force_indications"));
        if (it_force != args.end()) {
            force_indications = utils::from_value<bool>(&it_force->second);
        }

        Log("SetNotifyValueAsync: %s, svc: %s, chr: %s, inst: %d, enable: %d", 
            remote_id.c_str(), service_uuid_str.c_str(), characteristic_uuid_str.c_str(), instance_id, enable);

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
             Log("SetNotifyValueAsync: Device not connected");
             error_string = "Device not connected";
             goto send_event;
        }

        // Unique key for storing subscription info
        std::string token_key = remote_id + ":" + service_uuid_str + ":" + characteristic_uuid_str + ":" + std::to_string(instance_id);

        // Optimized Search by Handle (instance_id)
        if (instance_id != 0) {
            targetChar = co_await GetCharacteristicByHandleAsync(device, instance_id);
        }

        // Fallback to UUID search if handle matching failed (legacy/fallback)
        if (!targetChar) {
            Log("SetNotifyValueAsync: Characteristic not found by handle, trying UUIDs...");
            targetChar = co_await GetCharacteristicAsync(device, service_uuid_str, characteristic_uuid_str, primary_service_uuid_str, instance_id);
        }

        if (!targetChar) {
             Log("SetNotifyValueAsync: Characteristic not found");
             error_string = "Characteristic not found";
             goto send_event;
        }

        if (enable) {
            GattClientCharacteristicConfigurationDescriptorValue cccdValue = GattClientCharacteristicConfigurationDescriptorValue::None;
            auto props = targetChar.CharacteristicProperties();
            
            bool canNotify = (static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Notify)) != 0;
            bool canIndicate = (static_cast<uint32_t>(props) & static_cast<uint32_t>(GattCharacteristicProperties::Indicate)) != 0;

            if (force_indications && canIndicate) {
                cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
                return_value = {0x02, 0x00}; // Indicate
            } else if (canNotify) {
                cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Notify;
                return_value = {0x01, 0x00}; // Notify
            } else if (canIndicate) {
                cccdValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
                return_value = {0x02, 0x00}; // Indicate
            } else {
                 Log("SetNotifyValueAsync: Notify/Indicate not supported");
                 error_string = "Notify/Indicate not supported";
                 goto send_event;
            }

            GattCommunicationStatus status = GattCommunicationStatus::ProtocolError;
            try {
                status = co_await targetChar.WriteClientCharacteristicConfigurationDescriptorAsync(cccdValue);
            } catch (...) {
                Log("SetNotifyValueAsync: WriteClientCharacteristicConfigurationDescriptorAsync exception");
            }

            if (status != GattCommunicationStatus::Success) {
                Log("SetNotifyValueAsync: Failed to write CCCD via helper (%d), trying manual write...", status);
                
                try {
                    // Manual write fallback
                    GattDescriptor cccdDescriptor = nullptr;
                    winrt::guid cccdUuid = utils::parse_uuid("2902");

                    auto descResult = co_await targetChar.GetDescriptorsForUuidAsync(cccdUuid, BluetoothCacheMode::Cached);
                    if (descResult.Status() != GattCommunicationStatus::Success) {
                         descResult = co_await targetChar.GetDescriptorsForUuidAsync(cccdUuid, BluetoothCacheMode::Uncached);
                    }
                    
                    if (descResult.Status() == GattCommunicationStatus::Success && descResult.Descriptors().Size() > 0) {
                        cccdDescriptor = descResult.Descriptors().GetAt(0);
                    }

                    if (cccdDescriptor) {
                        auto writer = winrt::Windows::Storage::Streams::DataWriter();
                        writer.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
                        
                        uint16_t val = 0;
                        if (cccdValue == GattClientCharacteristicConfigurationDescriptorValue::Notify) val = 1;
                        else if (cccdValue == GattClientCharacteristicConfigurationDescriptorValue::Indicate) val = 2;
                        
                        writer.WriteUInt16(val);

                        auto writeResult = co_await cccdDescriptor.WriteValueWithResultAsync(writer.DetachBuffer());
                        status = writeResult.Status();
                        Log("SetNotifyValueAsync: Manual CCCD write status: %d", status);
                    } else {
                        Log("SetNotifyValueAsync: CCCD Descriptor not found manually");
                    }
                } catch(...) {
                    Log("SetNotifyValueAsync: Manual CCCD write exception");
                }
            }

            // [FIX] Ignore CCCD write error for "Service Changed" characteristic (0x2A05)
            // Windows OS manages this characteristic and often blocks direct writes.
            if (status != GattCommunicationStatus::Success && characteristic_uuid_str == "2a05") {
                Log("SetNotifyValueAsync: Ignoring CCCD write failure for Service Changed (2A05). Assuming OS handled.");
                status = GattCommunicationStatus::Success;
            }

            if (status == GattCommunicationStatus::Success) {
                Log("SetNotifyValueAsync: Write CCCD success");
                co_await ui_thread_;
                
                // Remove old subscription if exists
                auto it = subscribed_characteristics_.find(token_key);
                if (it != subscribed_characteristics_.end()) {
                     try {
                        it->second.characteristic.ValueChanged(it->second.token);
                     } catch(...) {}
                     subscribed_characteristics_.erase(it);
                }

                auto token = targetChar.ValueChanged([this, remote_id](GattCharacteristic const& sender, GattValueChangedEventArgs const& args) {
                    this->OnCharacteristicValueChanged(remote_id, sender, args);
                });
                subscribed_characteristics_[token_key] = { targetChar, token };
                
                success_event = true;
                error_string = "GATT_SUCCESS";
            } else {
                Log("SetNotifyValueAsync: Write CCCD failed final: %d", status);
                error_string = "Write CCCD failed";
            }
        } else {
             // Disable
             return_value = {0x00, 0x00};
             GattCommunicationStatus status = GattCommunicationStatus::ProtocolError;
             try {
                status = co_await targetChar.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::None);
             } catch(...) {
                 Log("SetNotifyValueAsync: Disable CCCD exception");
             }

             if (status != GattCommunicationStatus::Success) {
                 try {
                     // Try manual disable
                     GattDescriptor cccdDescriptor = nullptr;
                     winrt::guid cccdUuid = utils::parse_uuid("2902");
                     auto descResult = co_await targetChar.GetDescriptorsForUuidAsync(cccdUuid, BluetoothCacheMode::Cached);
                     if (descResult.Status() != GattCommunicationStatus::Success) {
                          descResult = co_await targetChar.GetDescriptorsForUuidAsync(cccdUuid, BluetoothCacheMode::Uncached);
                     }
                     if (descResult.Status() == GattCommunicationStatus::Success && descResult.Descriptors().Size() > 0) {
                         cccdDescriptor = descResult.Descriptors().GetAt(0);
                         auto writer = winrt::Windows::Storage::Streams::DataWriter();
                         writer.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
                         writer.WriteUInt16(0);
                         auto writeResult = co_await cccdDescriptor.WriteValueWithResultAsync(writer.DetachBuffer());
                         status = writeResult.Status();
                     }
                 } catch (...) {
                     Log("SetNotifyValueAsync: Manual disable CCCD exception");
                 }
             }

             // [FIX] Ignore CCCD write error for "Service Changed" characteristic (0x2A05) on Disable too
             if (status != GattCommunicationStatus::Success && characteristic_uuid_str == "2a05") {
                Log("SetNotifyValueAsync: Ignoring CCCD disable failure for Service Changed (2A05).");
                status = GattCommunicationStatus::Success;
             }

             co_await ui_thread_;
             auto it = subscribed_characteristics_.find(token_key);
             if (it != subscribed_characteristics_.end()) {
                 try {
                    it->second.characteristic.ValueChanged(it->second.token);
                 } catch(...) {}
                 subscribed_characteristics_.erase(it);
             }

             if (status == GattCommunicationStatus::Success) {
                Log("SetNotifyValueAsync: Disable CCCD success");
                success_event = true;
                error_string = "GATT_SUCCESS";
             } else {
                 Log("SetNotifyValueAsync: Disable CCCD failed: %d", status);
                 error_string = "Disable CCCD failed";
             }
        }
        
    } catch (const winrt::hresult_error& ex) {
        Log("SetNotifyValueAsync: WinRT Exception: 0x%08X %ls", ex.code(), ex.message().c_str());
        error_string = "WinRT Error: " + utils::to_string(ex.message());
    } catch (const std::exception& ex) {
        Log("SetNotifyValueAsync: Std Exception: %s", ex.what());
        error_string = std::string("Std Error: ") + ex.what();
    } catch (...) {
        Log("SetNotifyValueAsync: Unknown Exception occurred");
        error_string = "Unknown Exception occurred";
    }

send_event:
    co_await ui_thread_;
    flutter::EncodableMap response;
    response[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
    if (!primary_service_uuid_str.empty()) {
            response[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(primary_service_uuid_str);
    }
    response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid_str);
    response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid_str);
    response[flutter::EncodableValue("descriptor_uuid")] = flutter::EncodableValue("2902"); 
    response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id);
    
    response[flutter::EncodableValue("value")] = flutter::EncodableValue(return_value);
    response[flutter::EncodableValue("success")] = flutter::EncodableValue(success_event ? 1 : 0);
    response[flutter::EncodableValue("error_code")] = flutter::EncodableValue(success_event ? 0 : 1);
    response[flutter::EncodableValue("error_string")] = flutter::EncodableValue(error_string);

    if (channel_) {
        channel_->InvokeMethod("OnDescriptorWritten", std::make_unique<flutter::EncodableValue>(response));
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

        GattCharacteristic targetChar = nullptr;

        Log("ReadCharacteristicAsync: looking for handle %d", instance_id);

        // 1. Try to reuse subscribed characteristic object if available
        std::string token_key = remote_id + ":" + service_uuid_str + ":" + characteristic_uuid_str + ":" + std::to_string(instance_id);
        co_await ui_thread_;
        auto it_sub = subscribed_characteristics_.find(token_key);
        if (it_sub != subscribed_characteristics_.end()) {
            targetChar = it_sub->second.characteristic;
        }
        co_await winrt::resume_background();

        // 2. If not found, find via UUIDs/Handle
        if (!targetChar) {
             if (instance_id != 0) {
                  targetChar = co_await GetCharacteristicByHandleAsync(device, instance_id);
             }
             if (!targetChar) {
                 std::string primary_service_uuid_str = ""; 
                 auto it_primary = args.find(flutter::EncodableValue("primary_service_uuid"));
                 if (it_primary != args.end()) {
                    primary_service_uuid_str = utils::from_value<std::string>(&it_primary->second);
                 }
                 targetChar = co_await GetCharacteristicAsync(device, service_uuid_str, characteristic_uuid_str, primary_service_uuid_str, instance_id);
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
            // ... (Add primary service uuid if needed in response, omitted for brevity but good practice)
            response[flutter::EncodableValue("service_uuid")] = flutter::EncodableValue(service_uuid_str);
            response[flutter::EncodableValue("characteristic_uuid")] = flutter::EncodableValue(characteristic_uuid_str);
            response[flutter::EncodableValue("instance_id")] = flutter::EncodableValue(instance_id);
            response[flutter::EncodableValue("value")] = flutter::EncodableValue(value);
            response[flutter::EncodableValue("success")] = flutter::EncodableValue(1);
            response[flutter::EncodableValue("error_code")] = flutter::EncodableValue(0);
            response[flutter::EncodableValue("error_string")] = flutter::EncodableValue("GATT_SUCCESS");
            
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
        std::string primary_service_uuid_str;
        
        int instance_id = 0;
        auto it_instance = args.find(flutter::EncodableValue("instance_id"));
        if (it_instance != args.end()) {
            instance_id = utils::from_value<int>(&it_instance->second);
        }

        auto it_primary = args.find(flutter::EncodableValue("primary_service_uuid"));
        if (it_primary != args.end()) {
            primary_service_uuid_str = utils::from_value<std::string>(&it_primary->second);
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

        GattCharacteristic targetChar = nullptr;

        Log("WriteCharacteristicAsync: looking for handle %d", instance_id);

        // 1. Try to find characteristic via UUIDs/Handle (Fresh lookup to ensure validity)
        if (instance_id != 0) {
            targetChar = co_await GetCharacteristicByHandleAsync(device, instance_id);
        }

        // 2. Fallback to UUID lookup if handle failed
        if (!targetChar) {
             targetChar = co_await GetCharacteristicAsync(device, service_uuid_str, characteristic_uuid_str, primary_service_uuid_str, instance_id);
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
            if (!primary_service_uuid_str.empty()) {
                response[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(primary_service_uuid_str);
            }
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
            if (!primary_service_uuid_str.empty()) {
                response[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(primary_service_uuid_str);
            }
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
        std::string primary_service_uuid_str;
        
        int instance_id = 0; 
        auto it_instance = args.find(flutter::EncodableValue("instance_id"));
        if (it_instance != args.end()) {
            instance_id = utils::from_value<int>(&it_instance->second);
        }

        auto it_primary = args.find(flutter::EncodableValue("primary_service_uuid"));
        if (it_primary != args.end()) {
            primary_service_uuid_str = utils::from_value<std::string>(&it_primary->second);
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
             result_ptr->Error("readDescriptor", "device is disconnected");
             co_return;
        }

        co_await winrt::resume_background();

        GattDescriptor targetDesc = nullptr;
        GattCharacteristic targetChar = nullptr;

        // 1. Try to find characteristic by handle first
        // FBP sends descriptor handle in instance_id. 
        // We use GetCharacteristicByDescriptorHandleAsync to find the parent characteristic.
        if (instance_id != 0) {
             targetChar = co_await GetCharacteristicByDescriptorHandleAsync(device, instance_id);
             if (!targetChar) {
                 // Fallback: maybe it was characteristic handle?
                 targetChar = co_await GetCharacteristicByHandleAsync(device, instance_id);
             }
        }

        // 2. Fallback to UUID search if char not found by handle
        if (!targetChar) {
            targetChar = co_await GetCharacteristicAsync(device, service_uuid_str, characteristic_uuid_str, primary_service_uuid_str, 0);
        }

        if (!targetChar) {
            co_await ui_thread_;
            result_ptr->Error("readDescriptor", "Characteristic not found");
            co_return;
        }

        // 3. Find Descriptor inside Characteristic
        // If instance_id matches a descriptor handle, use it
        if (instance_id != 0) {
            auto descResult = co_await targetChar.GetDescriptorsAsync(BluetoothCacheMode::Cached);
            if (descResult.Status() == GattCommunicationStatus::Success) {
                for (auto d : descResult.Descriptors()) {
                    if (static_cast<int32_t>(d.AttributeHandle()) == instance_id) {
                        targetDesc = d;
                        break;
                    }
                }
            }
             if (!targetDesc) {
                descResult = co_await targetChar.GetDescriptorsAsync(BluetoothCacheMode::Uncached);
                 if (descResult.Status() == GattCommunicationStatus::Success) {
                    for (auto d : descResult.Descriptors()) {
                        if (static_cast<int32_t>(d.AttributeHandle()) == instance_id) {
                            targetDesc = d;
                            break;
                        }
                    }
                }
            }
        }

        // If no handle match (or handle was 0), try UUID match
        if (!targetDesc) {
             targetDesc = co_await GetDescriptorAsync(targetChar, descriptor_uuid_str);
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
            if (!primary_service_uuid_str.empty()) {
                response[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(primary_service_uuid_str);
            }
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
        std::string primary_service_uuid_str;

        int instance_id = 0; 
        auto it_instance = args.find(flutter::EncodableValue("instance_id"));
        if (it_instance != args.end()) {
            instance_id = utils::from_value<int>(&it_instance->second);
        }

        auto it_primary = args.find(flutter::EncodableValue("primary_service_uuid"));
        if (it_primary != args.end()) {
            primary_service_uuid_str = utils::from_value<std::string>(&it_primary->second);
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
             result_ptr->Error("writeDescriptor", "device is disconnected");
             co_return;
        }

        co_await winrt::resume_background();

        GattDescriptor targetDesc = nullptr;
        GattCharacteristic targetChar = nullptr;

        // 1. Try to find parent Characteristic using descriptor handle (instance_id)
        if (instance_id != 0) {
            targetChar = co_await GetCharacteristicByDescriptorHandleAsync(device, instance_id);
            
            if (!targetChar) {
                targetChar = co_await GetCharacteristicByHandleAsync(device, instance_id);
            }
        }

        // 2. Fallback to UUID search for Characteristic
        if (!targetChar) {
            targetChar = co_await GetCharacteristicAsync(device, service_uuid_str, characteristic_uuid_str, primary_service_uuid_str, 0);
        }

        if (!targetChar) {
            co_await ui_thread_;
            result_ptr->Error("writeDescriptor", "Characteristic not found");
            co_return;
        }

        // 3. Find Descriptor within Characteristic (by Handle first, then UUID)
         if (instance_id != 0) {
            auto descResult = co_await targetChar.GetDescriptorsAsync(BluetoothCacheMode::Cached);
            if (descResult.Status() == GattCommunicationStatus::Success) {
                for (auto d : descResult.Descriptors()) {
                    if (static_cast<int32_t>(d.AttributeHandle()) == instance_id) {
                        targetDesc = d;
                        break;
                    }
                }
            }
             if (!targetDesc) {
                descResult = co_await targetChar.GetDescriptorsAsync(BluetoothCacheMode::Uncached);
                 if (descResult.Status() == GattCommunicationStatus::Success) {
                    for (auto d : descResult.Descriptors()) {
                        if (static_cast<int32_t>(d.AttributeHandle()) == instance_id) {
                            targetDesc = d;
                            break;
                        }
                    }
                }
            }
        }

        if (!targetDesc) {
            targetDesc = co_await GetDescriptorAsync(targetChar, descriptor_uuid_str);
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
            if (!primary_service_uuid_str.empty()) {
                response[flutter::EncodableValue("primary_service_uuid")] = flutter::EncodableValue(primary_service_uuid_str);
            }
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
            
            // 1. Remove subscriptions for this device to release references
            for (auto it = subscribed_characteristics_.begin(); it != subscribed_characteristics_.end(); ) {
                if (it->first.find(remote_id) == 0) { // Key starts with remote_id
                    // No need to explicitly Unsubscribe if we are closing the device, 
                    // but releasing the GattCharacteristic object is crucial.
                    it = subscribed_characteristics_.erase(it);
                } else {
                    ++it;
                }
            }

            // 2. Remove from connected devices list
            auto it = std::find_if(connected_devices_.begin(), connected_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
            
            if (it != connected_devices_.end()) {
                auto device = it->second.as<BluetoothLEDevice>();
                if (device) {
                     device.Close(); 
                }
                connected_devices_.erase(it);
            }

            // 3. Remove from connecting list (if any)
            auto it_connecting = std::find_if(currently_connecting_devices_.begin(), currently_connecting_devices_.end(),
                [&](const auto& pair) { return pair.first == remote_id; });
            if (it_connecting != currently_connecting_devices_.end()) {
                auto device_connecting = it_connecting->second.as<BluetoothLEDevice>();
                if (device_connecting) {
                    device_connecting.Close(); 
                }
                currently_connecting_devices_.erase(it_connecting);
            }

            // 4. Send Disconnected Event
            flutter::EncodableMap connection_state;
            connection_state[flutter::EncodableValue("remote_id")] = flutter::EncodableValue(remote_id);
            connection_state[flutter::EncodableValue("connection_state")] = flutter::EncodableValue(0); 
            channel_->InvokeMethod("OnConnectionStateChanged", std::make_unique<flutter::EncodableValue>(connection_state));
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