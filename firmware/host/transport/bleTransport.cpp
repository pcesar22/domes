/**
 * @file bleTransport.cpp
 * @brief BLE transport implementation using BlueZ D-Bus API
 */

#include "bleTransport.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include <gio/gio.h>

namespace domes::host {

// ============================================================================
// D-Bus State (internal)
// ============================================================================

struct BleTransport::DBusState {
    GDBusConnection* conn = nullptr;
    GMainLoop* loop = nullptr;
    GThread* loopThread = nullptr;
    guint propChangedSignal = 0;
    guint intfAddedSignal = 0;

    // Characteristic interfaces
    GDBusProxy* dataCharProxy = nullptr;
    GDBusProxy* statusCharProxy = nullptr;
    GDBusProxy* deviceProxy = nullptr;

    ~DBusState() {
        if (dataCharProxy) g_object_unref(dataCharProxy);
        if (statusCharProxy) g_object_unref(statusCharProxy);
        if (deviceProxy) g_object_unref(deviceProxy);

        if (propChangedSignal && conn) {
            g_dbus_connection_signal_unsubscribe(conn, propChangedSignal);
        }
        if (intfAddedSignal && conn) {
            g_dbus_connection_signal_unsubscribe(conn, intfAddedSignal);
        }

        if (loop) {
            g_main_loop_quit(loop);
            if (loopThread) {
                g_thread_join(loopThread);
            }
            g_main_loop_unref(loop);
        }

        if (conn) g_object_unref(conn);
    }
};

// ============================================================================
// Helper functions
// ============================================================================

static std::string variantToString(GVariant* variant) {
    if (!variant) return "";

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_STRING)) {
        return g_variant_get_string(variant, nullptr);
    }
    return "";
}

static std::vector<uint8_t> variantToByteArray(GVariant* variant) {
    std::vector<uint8_t> result;
    if (!variant) return result;

    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BYTESTRING) ||
        g_variant_is_of_type(variant, G_VARIANT_TYPE("ay"))) {
        gsize len = 0;
        const uint8_t* data = static_cast<const uint8_t*>(
            g_variant_get_fixed_array(variant, &len, sizeof(uint8_t)));
        if (data && len > 0) {
            result.assign(data, data + len);
        }
    }
    return result;
}

/// Read a D-Bus property directly (bypasses cache)
static GVariant* getPropertyDirect(GDBusConnection* conn,
                                   const char* objPath,
                                   const char* iface,
                                   const char* propName) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        conn,
        "org.bluez",
        objPath,
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", iface, propName),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        nullptr,
        &error);

    if (error) {
        g_error_free(error);
        return nullptr;
    }

    GVariant* value = nullptr;
    g_variant_get(result, "(v)", &value);
    g_variant_unref(result);
    return value;
}

// ============================================================================
// BleTransport Implementation
// ============================================================================

BleTransport::BleTransport(const std::string& targetName,
                           const std::string& targetAddress)
    : targetName_(targetName),
      targetAddress_(targetAddress),
      dbus_(std::make_unique<DBusState>()) {}

BleTransport::~BleTransport() {
    disconnect();
}

TransportError BleTransport::init() {
    if (initialized_) {
        return TransportError::kAlreadyInit;
    }

    std::cout << "[BLE] Initializing BLE transport..." << std::endl;

    // Initialize D-Bus
    if (!initDBus()) {
        std::cerr << "[BLE] Failed to initialize D-Bus" << std::endl;
        return TransportError::kIoError;
    }

    // Enable Bluetooth adapter
    if (!enableAdapter()) {
        std::cerr << "[BLE] Failed to enable Bluetooth adapter" << std::endl;
        return TransportError::kIoError;
    }

    // Scan for device if address not provided
    if (targetAddress_.empty()) {
        std::cout << "[BLE] Scanning for device: " << targetName_ << std::endl;
        deviceAddress_ = scanForDevice(15);
        if (deviceAddress_.empty()) {
            std::cerr << "[BLE] Device not found: " << targetName_ << std::endl;
            return TransportError::kTimeout;
        }
    } else {
        deviceAddress_ = targetAddress_;
    }

    std::cout << "[BLE] Found device at: " << deviceAddress_ << std::endl;

    // Connect to device
    if (!connectToDevice()) {
        std::cerr << "[BLE] Failed to connect to device" << std::endl;
        return TransportError::kIoError;
    }

    // Discover services and characteristics
    if (!discoverServices()) {
        std::cerr << "[BLE] Failed to discover services" << std::endl;
        disconnect();
        return TransportError::kIoError;
    }

    // Setup characteristic proxies
    if (!setupCharacteristics()) {
        std::cerr << "[BLE] Failed to setup characteristics" << std::endl;
        disconnect();
        return TransportError::kIoError;
    }

    // Enable notifications on Status characteristic
    if (!enableNotifications()) {
        std::cerr << "[BLE] Failed to enable notifications" << std::endl;
        disconnect();
        return TransportError::kIoError;
    }

    initialized_ = true;
    std::cout << "[BLE] BLE transport initialized successfully" << std::endl;

    return TransportError::kOk;
}

TransportError BleTransport::send(const uint8_t* data, size_t len) {
    if (!initialized_ || !connected_) {
        return TransportError::kNotInitialized;
    }

    if (!data || len == 0) {
        return TransportError::kInvalidArg;
    }

    if (!dbus_->dataCharProxy) {
        return TransportError::kIoError;
    }

    // Build byte array from raw data
    GVariant* dataVariant = g_variant_new_fixed_array(
        G_VARIANT_TYPE_BYTE, data, len, sizeof(uint8_t));

    // Build options dict - use "request" for write-with-response (more reliable)
    GVariantBuilder optBuilder;
    g_variant_builder_init(&optBuilder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&optBuilder, "{sv}", "type",
                          g_variant_new_string("request"));

    // Call WriteValue method: WriteValue(ay data, a{sv} options)
    GError* error = nullptr;
    GVariant* result = g_dbus_proxy_call_sync(
        dbus_->dataCharProxy,
        "WriteValue",
        g_variant_new("(@aya{sv})", dataVariant, &optBuilder),
        G_DBUS_CALL_FLAGS_NONE,
        5000,  // 5 second timeout
        nullptr,
        &error);

    if (error) {
        std::cerr << "[BLE] WriteValue failed: " << error->message << std::endl;
        g_error_free(error);
        return TransportError::kIoError;
    }

    if (result) {
        g_variant_unref(result);
    }

    std::cout << "[BLE] WriteValue succeeded, sent " << len << " bytes" << std::endl;
    return TransportError::kOk;
}

TransportError BleTransport::receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) {
    if (!initialized_) {
        return TransportError::kNotInitialized;
    }

    if (!buf || !len || *len == 0) {
        return TransportError::kInvalidArg;
    }

    std::unique_lock<std::mutex> lock(rxMutex_);

    // Wait for data
    if (rxQueue_.empty()) {
        if (timeoutMs == 0) {
            *len = 0;
            return TransportError::kTimeout;
        }

        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs);

        if (!rxCv_.wait_until(lock, deadline, [this] { return !rxQueue_.empty(); })) {
            *len = 0;
            return TransportError::kTimeout;
        }
    }

    // Copy data from queue
    auto& front = rxQueue_.front();
    size_t toCopy = std::min(*len, front.size());
    std::memcpy(buf, front.data(), toCopy);
    *len = toCopy;

    rxQueue_.pop();

    return TransportError::kOk;
}

bool BleTransport::isConnected() const {
    return initialized_ && connected_;
}

void BleTransport::disconnect() {
    if (!dbus_ || !dbus_->conn) {
        return;
    }

    if (dbus_->deviceProxy && connected_) {
        GError* error = nullptr;
        GVariant* result = g_dbus_proxy_call_sync(
            dbus_->deviceProxy,
            "Disconnect",
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            5000,
            nullptr,
            &error);

        if (error) {
            g_error_free(error);
        }
        if (result) {
            g_variant_unref(result);
        }
    }

    connected_ = false;
    initialized_ = false;
}

std::string BleTransport::scanForDevice(int timeoutSec) {
    if (!dbus_ || !dbus_->conn) {
        return "";
    }

    // Start scanning
    if (!startScan()) {
        return "";
    }

    std::string foundAddress;
    auto startTime = std::chrono::steady_clock::now();

    // Poll for devices
    while (std::chrono::steady_clock::now() - startTime <
           std::chrono::seconds(timeoutSec)) {

        // Get managed objects
        GError* error = nullptr;
        GVariant* result = g_dbus_connection_call_sync(
            dbus_->conn,
            "org.bluez",
            "/",
            "org.freedesktop.DBus.ObjectManager",
            "GetManagedObjects",
            nullptr,
            G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
            G_DBUS_CALL_FLAGS_NONE,
            5000,
            nullptr,
            &error);

        if (error) {
            g_error_free(error);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // Parse result to find device
        GVariantIter* objIter;
        g_variant_get(result, "(a{oa{sa{sv}}})", &objIter);

        const char* objPath;
        GVariant* interfaces;
        while (g_variant_iter_next(objIter, "{&o@a{sa{sv}}}", &objPath, &interfaces)) {
            // Check if this is a Device1 interface
            GVariantIter* ifaceIter;
            g_variant_get(interfaces, "a{sa{sv}}", &ifaceIter);

            const char* ifaceName;
            GVariant* props;
            while (g_variant_iter_next(ifaceIter, "{&s@a{sv}}", &ifaceName, &props)) {
                if (std::string(ifaceName) == "org.bluez.Device1") {
                    // Check Name property
                    GVariant* nameProp = g_variant_lookup_value(props, "Name",
                                                                 G_VARIANT_TYPE_STRING);
                    if (nameProp) {
                        std::string name = variantToString(nameProp);
                        if (name == targetName_) {
                            // Check for OTA Service UUID
                            GVariant* uuidsProp = g_variant_lookup_value(props, "UUIDs",
                                                                          G_VARIANT_TYPE_STRING_ARRAY);
                            if (uuidsProp) {
                                GVariantIter uuidIter;
                                g_variant_iter_init(&uuidIter, uuidsProp);
                                const char* uuid;
                                while (g_variant_iter_next(&uuidIter, "&s", &uuid)) {
                                    if (std::string(uuid) == kOtaServiceUuid) {
                                        // Found the device!
                                        GVariant* addrProp = g_variant_lookup_value(props, "Address",
                                                                                     G_VARIANT_TYPE_STRING);
                                        if (addrProp) {
                                            foundAddress = variantToString(addrProp);
                                            devicePath_ = objPath;
                                            g_variant_unref(addrProp);
                                        }
                                        g_variant_unref(uuidsProp);
                                        g_variant_unref(nameProp);
                                        g_variant_unref(props);
                                        g_variant_iter_free(ifaceIter);
                                        g_variant_unref(interfaces);
                                        g_variant_iter_free(objIter);
                                        g_variant_unref(result);
                                        stopScan();
                                        return foundAddress;
                                    }
                                }
                                g_variant_unref(uuidsProp);
                            }
                        }
                        g_variant_unref(nameProp);
                    }
                }
                g_variant_unref(props);
            }
            g_variant_iter_free(ifaceIter);
            g_variant_unref(interfaces);
        }
        g_variant_iter_free(objIter);
        g_variant_unref(result);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    stopScan();
    return foundAddress;
}

void BleTransport::onNotification(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(rxMutex_);
    rxQueue_.emplace(data, data + len);
    rxCv_.notify_one();
}

void BleTransport::onConnectionChanged(bool connected) {
    connected_ = connected;
    if (!connected) {
        std::cerr << "[BLE] Device disconnected" << std::endl;
    }
}

// ============================================================================
// D-Bus Helper Methods
// ============================================================================

static gpointer mainLoopThreadFunc(gpointer data) {
    GMainLoop* loop = static_cast<GMainLoop*>(data);
    g_main_loop_run(loop);
    return nullptr;
}

bool BleTransport::initDBus() {
    GError* error = nullptr;

    dbus_->conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (error) {
        std::cerr << "[BLE] Failed to connect to system bus: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // Create and start GLib main loop for D-Bus signal handling
    // Use default context so signals on the system bus are processed
    dbus_->loop = g_main_loop_new(nullptr, FALSE);
    dbus_->loopThread = g_thread_new("dbus-loop", mainLoopThreadFunc, dbus_->loop);

    std::cout << "[BLE] D-Bus main loop started" << std::endl;
    return true;
}

void BleTransport::cleanupDBus() {
    dbus_.reset();
}

bool BleTransport::enableAdapter() {
    GError* error = nullptr;

    // Get adapter proxy (typically hci0)
    GDBusProxy* adapterProxy = g_dbus_proxy_new_sync(
        dbus_->conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        "/org/bluez/hci0",
        "org.bluez.Adapter1",
        nullptr,
        &error);

    if (error) {
        std::cerr << "[BLE] Failed to get adapter proxy: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // Check if powered
    GVariant* powered = g_dbus_proxy_get_cached_property(adapterProxy, "Powered");
    if (powered && !g_variant_get_boolean(powered)) {
        // Power on adapter
        GVariant* result = g_dbus_proxy_call_sync(
            adapterProxy,
            "org.freedesktop.DBus.Properties.Set",
            g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered",
                          g_variant_new_boolean(TRUE)),
            G_DBUS_CALL_FLAGS_NONE,
            5000,
            nullptr,
            &error);

        if (error) {
            std::cerr << "[BLE] Failed to power on adapter: " << error->message << std::endl;
            g_error_free(error);
            g_variant_unref(powered);
            g_object_unref(adapterProxy);
            return false;
        }
        if (result) g_variant_unref(result);
    }
    if (powered) g_variant_unref(powered);

    g_object_unref(adapterProxy);
    return true;
}

bool BleTransport::startScan() {
    GError* error = nullptr;

    GDBusProxy* adapterProxy = g_dbus_proxy_new_sync(
        dbus_->conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        "/org/bluez/hci0",
        "org.bluez.Adapter1",
        nullptr,
        &error);

    if (error) {
        g_error_free(error);
        return false;
    }

    // Set discovery filter for LE only
    GVariantBuilder filterBuilder;
    g_variant_builder_init(&filterBuilder, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&filterBuilder, "{sv}", "Transport",
                          g_variant_new_string("le"));

    GVariant* result = g_dbus_proxy_call_sync(
        adapterProxy,
        "SetDiscoveryFilter",
        g_variant_new("(a{sv})", &filterBuilder),
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        nullptr,
        nullptr);  // Ignore errors

    if (result) g_variant_unref(result);

    // Start discovery
    result = g_dbus_proxy_call_sync(
        adapterProxy,
        "StartDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        nullptr,
        &error);

    g_object_unref(adapterProxy);

    if (error) {
        // Ignore "already discovering" error
        if (strstr(error->message, "Already") == nullptr) {
            std::cerr << "[BLE] StartDiscovery failed: " << error->message << std::endl;
        }
        g_error_free(error);
        // Continue anyway
    }

    if (result) g_variant_unref(result);
    return true;
}

bool BleTransport::stopScan() {
    GError* error = nullptr;

    GDBusProxy* adapterProxy = g_dbus_proxy_new_sync(
        dbus_->conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        "/org/bluez/hci0",
        "org.bluez.Adapter1",
        nullptr,
        &error);

    if (error) {
        g_error_free(error);
        return false;
    }

    GVariant* result = g_dbus_proxy_call_sync(
        adapterProxy,
        "StopDiscovery",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        nullptr,
        nullptr);  // Ignore errors

    g_object_unref(adapterProxy);

    if (result) g_variant_unref(result);
    return true;
}

bool BleTransport::connectToDevice() {
    if (devicePath_.empty()) {
        // Build device path from address
        std::string addrPath = deviceAddress_;
        std::replace(addrPath.begin(), addrPath.end(), ':', '_');
        devicePath_ = "/org/bluez/hci0/dev_" + addrPath;
    }

    std::cout << "[BLE] Connecting to: " << devicePath_ << std::endl;

    GError* error = nullptr;

    dbus_->deviceProxy = g_dbus_proxy_new_sync(
        dbus_->conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        devicePath_.c_str(),
        "org.bluez.Device1",
        nullptr,
        &error);

    if (error) {
        std::cerr << "[BLE] Failed to get device proxy: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    // Connect
    GVariant* result = g_dbus_proxy_call_sync(
        dbus_->deviceProxy,
        "Connect",
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        30000,  // 30 second timeout
        nullptr,
        &error);

    if (error) {
        std::cerr << "[BLE] Connect failed: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    if (result) g_variant_unref(result);

    // Wait for connection (use direct D-Bus property read, not cached)
    for (int i = 0; i < 20; i++) {
        GVariant* connected = getPropertyDirect(
            dbus_->conn, devicePath_.c_str(), "org.bluez.Device1", "Connected");
        if (connected && g_variant_get_boolean(connected)) {
            connected_ = true;
            g_variant_unref(connected);
            std::cout << "[BLE] Connected!" << std::endl;
            return true;
        }
        if (connected) g_variant_unref(connected);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::cerr << "[BLE] Connection timeout" << std::endl;
    return false;
}

bool BleTransport::discoverServices() {
    std::cout << "[BLE] Waiting for service discovery..." << std::endl;

    // Wait for ServicesResolved (use direct D-Bus property read)
    for (int i = 0; i < 40; i++) {  // 10 seconds
        GVariant* resolved = getPropertyDirect(
            dbus_->conn, devicePath_.c_str(), "org.bluez.Device1", "ServicesResolved");
        if (resolved && g_variant_get_boolean(resolved)) {
            g_variant_unref(resolved);
            std::cout << "[BLE] Services resolved" << std::endl;
            return true;
        }
        if (resolved) g_variant_unref(resolved);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::cerr << "[BLE] Service discovery timeout" << std::endl;
    return false;
}

bool BleTransport::setupCharacteristics() {
    // Find characteristics by UUID
    GError* error = nullptr;

    GVariant* result = g_dbus_connection_call_sync(
        dbus_->conn,
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects",
        nullptr,
        G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        nullptr,
        &error);

    if (error) {
        std::cerr << "[BLE] GetManagedObjects failed: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    GVariantIter* objIter;
    g_variant_get(result, "(a{oa{sa{sv}}})", &objIter);

    const char* objPath;
    GVariant* interfaces;
    while (g_variant_iter_next(objIter, "{&o@a{sa{sv}}}", &objPath, &interfaces)) {
        // Must be under our device path
        if (std::string(objPath).find(devicePath_) != 0) {
            g_variant_unref(interfaces);
            continue;
        }

        GVariantIter* ifaceIter;
        g_variant_get(interfaces, "a{sa{sv}}", &ifaceIter);

        const char* ifaceName;
        GVariant* props;
        while (g_variant_iter_next(ifaceIter, "{&s@a{sv}}", &ifaceName, &props)) {
            if (std::string(ifaceName) == "org.bluez.GattCharacteristic1") {
                GVariant* uuidProp = g_variant_lookup_value(props, "UUID",
                                                            G_VARIANT_TYPE_STRING);
                if (uuidProp) {
                    std::string uuid = variantToString(uuidProp);

                    if (uuid == kOtaDataCharUuid) {
                        dataCharPath_ = objPath;
                        std::cout << "[BLE] Found Data characteristic: " << objPath << std::endl;
                    } else if (uuid == kOtaStatusCharUuid) {
                        statusCharPath_ = objPath;
                        std::cout << "[BLE] Found Status characteristic: " << objPath << std::endl;
                    }

                    g_variant_unref(uuidProp);
                }
            }
            g_variant_unref(props);
        }
        g_variant_iter_free(ifaceIter);
        g_variant_unref(interfaces);
    }
    g_variant_iter_free(objIter);
    g_variant_unref(result);

    if (dataCharPath_.empty() || statusCharPath_.empty()) {
        std::cerr << "[BLE] OTA characteristics not found" << std::endl;
        return false;
    }

    // Create characteristic proxies
    dbus_->dataCharProxy = g_dbus_proxy_new_sync(
        dbus_->conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        dataCharPath_.c_str(),
        "org.bluez.GattCharacteristic1",
        nullptr,
        &error);

    if (error) {
        std::cerr << "[BLE] Failed to create data char proxy: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    dbus_->statusCharProxy = g_dbus_proxy_new_sync(
        dbus_->conn,
        G_DBUS_PROXY_FLAGS_NONE,
        nullptr,
        "org.bluez",
        statusCharPath_.c_str(),
        "org.bluez.GattCharacteristic1",
        nullptr,
        &error);

    if (error) {
        std::cerr << "[BLE] Failed to create status char proxy: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    return true;
}

// Global callback for PropertiesChanged signal
static void onPropertiesChanged(GDBusConnection* /*connection*/,
                                 const gchar* /*sender_name*/,
                                 const gchar* object_path,
                                 const gchar* /*interface_name*/,
                                 const gchar* /*signal_name*/,
                                 GVariant* parameters,
                                 gpointer user_data) {
    BleTransport* transport = static_cast<BleTransport*>(user_data);

    std::cout << "[BLE] PropertiesChanged signal received on: " << object_path << std::endl;

    const char* iface;
    GVariant* changed;
    g_variant_get(parameters, "(&s@a{sv}as)", &iface, &changed, nullptr);

    std::cout << "[BLE] Interface: " << iface << std::endl;

    if (std::string(iface) == "org.bluez.GattCharacteristic1") {
        GVariant* value = g_variant_lookup_value(changed, "Value",
                                                  G_VARIANT_TYPE("ay"));
        if (value) {
            std::vector<uint8_t> data = variantToByteArray(value);
            std::cout << "[BLE] Notification received: " << data.size() << " bytes" << std::endl;
            if (!data.empty()) {
                transport->onNotification(data.data(), data.size());
            }
            g_variant_unref(value);
        } else {
            std::cout << "[BLE] No 'Value' in changed properties" << std::endl;
        }
    }

    g_variant_unref(changed);
}

bool BleTransport::enableNotifications() {
    // Verify we're still connected
    GVariant* connStatus = getPropertyDirect(
        dbus_->conn, devicePath_.c_str(), "org.bluez.Device1", "Connected");
    if (!connStatus || !g_variant_get_boolean(connStatus)) {
        std::cerr << "[BLE] Device disconnected before enabling notifications" << std::endl;
        if (connStatus) g_variant_unref(connStatus);
        return false;
    }
    g_variant_unref(connStatus);
    std::cout << "[BLE] Connection verified, enabling notifications..." << std::endl;

    // Give BlueZ time to stabilize GATT connection
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Subscribe to PropertiesChanged signal for notifications
    // Subscribe broadly to catch all BlueZ property changes on our device
    dbus_->propChangedSignal = g_dbus_connection_signal_subscribe(
        dbus_->conn,
        "org.bluez",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        nullptr,  // Subscribe to all paths (filter in callback)
        "org.bluez.GattCharacteristic1",  // Filter by interface
        G_DBUS_SIGNAL_FLAGS_NONE,
        onPropertiesChanged,
        this,
        nullptr);

    std::cout << "[BLE] Subscribed to GattCharacteristic1 PropertiesChanged signals" << std::endl;

    // Start notifications using direct D-Bus call (more reliable than proxy)
    std::cout << "[BLE] Calling StartNotify on: " << statusCharPath_ << std::endl;
    GError* error = nullptr;

    // Retry loop for GATT stabilization
    for (int attempt = 0; attempt < 5; attempt++) {
        error = nullptr;
        GVariant* result = g_dbus_connection_call_sync(
            dbus_->conn,
            "org.bluez",
            statusCharPath_.c_str(),
            "org.bluez.GattCharacteristic1",
            "StartNotify",
            nullptr,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            5000,
            nullptr,
            &error);

        if (!error) {
            if (result) g_variant_unref(result);
            std::cout << "[BLE] Notifications enabled on Status characteristic" << std::endl;
            return true;
        }

        std::cerr << "[BLE] StartNotify attempt " << (attempt + 1) << " failed: "
                  << error->message << std::endl;
        g_error_free(error);

        // Wait and retry
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    std::cerr << "[BLE] Failed to enable notifications after retries" << std::endl;
    return false;
}

}  // namespace domes::host
