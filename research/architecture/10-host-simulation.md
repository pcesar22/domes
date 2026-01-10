# 10 - Host Simulation Architecture

## Overview

This document describes how to use a Linux host (WSL2/Ubuntu) to simulate pod behavior for development and testing. The goal is maximum code reuse between host simulation and actual ESP32 firmware.

---

## Why Host Simulation?

| Problem | Solution |
|---------|----------|
| Can't test OTA flow without phone app | Host simulates phone |
| Can't test multi-pod coordination | Host simulates master pod |
| Slow flash/debug cycle on ESP32 | Fast iteration on host |
| Need to test protocol edge cases | Easy to inject errors on host |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        DEVELOPMENT TOPOLOGY                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PHASE 1: Serial Transport (Simple)                                         │
│  ┌──────────────┐         Serial/USB         ┌──────────────┐               │
│  │ Host         │ ─────────────────────────► │ Real ESP32   │               │
│  │ OTA Sender   │      /dev/ttyACM0          │ Pod          │               │
│  └──────────────┘                            └──────────────┘               │
│                                                                              │
│  PHASE 2: TCP Simulation (Multi-pod)                                        │
│  ┌──────────────┐    TCP     ┌──────────────┐   Serial   ┌──────────────┐  │
│  │ Phone        │ ─────────► │ Host         │ ─────────► │ Real ESP32   │  │
│  │ Simulator    │   :8080    │ Master Sim   │            │ Pods         │  │
│  └──────────────┘            └──────────────┘            └──────────────┘  │
│                                                                              │
│  PHASE 3: Real BLE (Indistinguishable from Phone)                           │
│  ┌──────────────┐    BLE     ┌──────────────┐  ESP-NOW   ┌──────────────┐  │
│  │ Host         │ ─────────► │ Real ESP32   │ ─────────► │ Real ESP32   │  │
│  │ BLE Master   │  (BlueZ)   │ Master Pod   │            │ Other Pods   │  │
│  └──────────────┘            └──────────────┘            └──────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Code Reuse Strategy

### Interface Abstraction

All communication goes through abstract interfaces:

```cpp
// interfaces/iTransport.hpp
class ITransport {
public:
    virtual ~ITransport() = default;

    virtual esp_err_t init() = 0;
    virtual esp_err_t send(const uint8_t* data, size_t len) = 0;
    virtual esp_err_t receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) = 0;
    virtual bool isConnected() const = 0;
    virtual void disconnect() = 0;
};
```

### Implementation Matrix

| Interface | ESP32 Implementation | Host Implementation |
|-----------|---------------------|---------------------|
| `ITransport` (phone↔master) | `BleTransport` | `TcpTransport` or `BleHostTransport` |
| `ITransport` (master↔pods) | `EspNowTransport` | `SerialTransport` |
| `ILedDriver` | `LedStripDriver` | `MockLedDriver` (console) |
| `IAudioDriver` | `Max98357Driver` | `MockAudioDriver` (WAV file) |
| `IHapticDriver` | `Drv2605Driver` | `MockHapticDriver` (no-op) |
| `IOtaManager` | `OtaManager` | Same (shared) |

### Shared vs Platform-Specific

```
firmware/
├── common/                      # SHARED (70-80% of code)
│   ├── interfaces/              # Abstract base classes
│   │   ├── iTransport.hpp
│   │   ├── iOtaManager.hpp
│   │   └── ...
│   ├── protocol/                # Message definitions & parsing
│   │   ├── ota_protocol.hpp     # OTA_BEGIN, OTA_DATA, OTA_END
│   │   ├── ota_protocol.cpp
│   │   ├── game_protocol.hpp    # Game commands
│   │   └── game_protocol.cpp
│   ├── services/                # Platform-agnostic business logic
│   │   ├── otaReceiver.hpp      # Receives OTA, writes to storage
│   │   ├── otaReceiver.cpp
│   │   └── ...
│   └── utils/                   # Helpers
│       ├── crc32.hpp
│       └── ringBuffer.hpp
│
├── domes/                       # ESP32-SPECIFIC
│   ├── main/
│   │   ├── transport/
│   │   │   ├── bleTransport.cpp
│   │   │   └── espNowTransport.cpp
│   │   ├── drivers/
│   │   │   ├── ledStrip.cpp
│   │   │   └── ...
│   │   └── platform/
│   │       └── esp32Storage.cpp  # esp_ota_write wrapper
│   └── ...
│
└── host/                        # HOST-SPECIFIC
    ├── CMakeLists.txt           # Standard CMake (not ESP-IDF)
    ├── main.cpp
    ├── transport/
    │   ├── tcpTransport.cpp     # TCP socket
    │   ├── serialTransport.cpp  # USB serial to ESP32
    │   └── bleHostTransport.cpp # BlueZ BLE (Phase 3)
    ├── mocks/
    │   ├── mockLedDriver.cpp    # Prints colors to console
    │   ├── mockAudioDriver.cpp  # Plays WAV or silent
    │   └── mockStorage.cpp      # Writes to file instead of flash
    └── sim/
        ├── phoneSimulator.cpp   # Simulates phone app
        └── masterSimulator.cpp  # Simulates master pod
```

---

## Phase 1: Serial OTA Sender

**Goal**: Send OTA updates directly to ESP32 via USB serial.

### Protocol Over Serial

```
┌────────────┬────────────┬────────────┬────────────┐
│ Start (2B) │ Length (2B)│ Type (1B)  │ Payload    │ CRC (4B) │
│ 0xAA 0x55  │ Little-end │            │ Variable   │          │
└────────────┴────────────┴────────────┴────────────┴──────────┘

Message Types:
  0x01 = OTA_BEGIN   { uint32_t size, uint8_t sha256[32], char version[32] }
  0x02 = OTA_DATA    { uint32_t offset, uint16_t len, uint8_t data[...] }
  0x03 = OTA_END     { }
  0x04 = OTA_ACK     { uint8_t status, uint32_t offset }
  0x05 = OTA_ABORT   { uint8_t reason }
```

### Host Implementation

```cpp
// host/simple_ota_sender.cpp
#include "protocol/ota_protocol.hpp"
#include "transport/serialTransport.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <port> <firmware.bin>\n", argv[0]);
        return 1;
    }

    SerialTransport serial(argv[1], 115200);
    if (serial.init() != 0) {
        fprintf(stderr, "Failed to open serial port\n");
        return 1;
    }

    // Read firmware file
    std::vector<uint8_t> firmware = readFile(argv[2]);
    uint8_t sha256[32];
    computeSha256(firmware.data(), firmware.size(), sha256);

    // Send OTA_BEGIN
    OtaBeginMsg begin = {
        .size = firmware.size(),
        .version = "v1.0.0"
    };
    memcpy(begin.sha256, sha256, 32);

    serial.sendMessage(MSG_OTA_BEGIN, &begin, sizeof(begin));

    // Wait for ACK
    OtaAckMsg ack;
    if (!serial.waitForAck(&ack, 5000)) {
        fprintf(stderr, "No ACK for OTA_BEGIN\n");
        return 1;
    }

    // Send chunks
    const size_t CHUNK_SIZE = 1024;
    for (size_t offset = 0; offset < firmware.size(); offset += CHUNK_SIZE) {
        size_t len = std::min(CHUNK_SIZE, firmware.size() - offset);

        OtaDataMsg data = {
            .offset = offset,
            .len = len
        };
        memcpy(data.data, firmware.data() + offset, len);

        serial.sendMessage(MSG_OTA_DATA, &data, sizeof(data.offset) + sizeof(data.len) + len);

        // Wait for ACK
        if (!serial.waitForAck(&ack, 1000)) {
            fprintf(stderr, "No ACK for chunk at offset %zu\n", offset);
            return 1;
        }

        printf("\rProgress: %zu / %zu bytes (%.1f%%)",
               offset + len, firmware.size(),
               100.0 * (offset + len) / firmware.size());
        fflush(stdout);
    }

    // Send OTA_END
    serial.sendMessage(MSG_OTA_END, nullptr, 0);

    printf("\nOTA complete! Device will reboot.\n");
    return 0;
}
```

### ESP32 Serial Receiver

```cpp
// domes/main/transport/serialOtaReceiver.cpp
// Add to existing firmware - listens on UART for OTA commands

void serialOtaTask(void* param) {
    SerialProtocol serial(UART_NUM_0);  // USB-JTAG UART
    OtaReceiver ota;

    while (true) {
        Message msg;
        if (serial.receive(&msg, portMAX_DELAY)) {
            switch (msg.type) {
                case MSG_OTA_BEGIN:
                    ota.begin(msg.payload.otaBegin);
                    serial.sendAck(0, 0);
                    break;

                case MSG_OTA_DATA:
                    ota.writeChunk(msg.payload.otaData);
                    serial.sendAck(0, msg.payload.otaData.offset);
                    break;

                case MSG_OTA_END:
                    ota.finish();  // Reboots
                    break;
            }
        }
    }
}
```

### Build & Run

```bash
# Build host tool
cd firmware/host
mkdir build && cd build
cmake ..
make

# Send OTA to device
./simple_ota_sender /dev/ttyACM0 ../../domes/build/domes.bin
```

---

## Phase 2: TCP Master Simulation

**Goal**: Simulate master pod on host, relay to real pods via serial.

### Architecture

```
┌─────────────────┐      TCP:8080      ┌─────────────────┐
│ Phone Simulator │ ─────────────────► │ Host Master Sim │
│ (or real phone) │                    │                 │
└─────────────────┘                    │  Translates:    │
                                       │  TCP → Serial   │
                                       │                 │
                                       └────────┬────────┘
                                                │
                              ┌─────────────────┼─────────────────┐
                              │                 │                 │
                              ▼                 ▼                 ▼
                       /dev/ttyACM0      /dev/ttyACM1      /dev/ttyACM2
                       ┌──────────┐      ┌──────────┐      ┌──────────┐
                       │  Pod 1   │      │  Pod 2   │      │  Pod 3   │
                       └──────────┘      └──────────┘      └──────────┘
```

### Host Master Simulator

```cpp
// host/sim/masterSimulator.cpp
class MasterSimulator {
public:
    MasterSimulator(int tcpPort, std::vector<std::string> serialPorts)
        : phoneTransport_(tcpPort)
    {
        for (const auto& port : serialPorts) {
            podTransports_.push_back(std::make_unique<SerialTransport>(port));
        }
    }

    void run() {
        printf("Master simulator listening on port %d\n", phoneTransport_.port());
        printf("Connected to %zu pods\n", podTransports_.size());

        while (true) {
            // Wait for connection from phone (or phone simulator)
            phoneTransport_.acceptConnection();

            // Handle OTA session
            handleOtaSession();
        }
    }

private:
    void handleOtaSession() {
        Message msg;

        while (phoneTransport_.receive(&msg)) {
            switch (msg.type) {
                case MSG_OTA_BEGIN:
                    // Forward to all pods
                    for (auto& pod : podTransports_) {
                        pod->sendMessage(msg);
                    }
                    // Collect ACKs
                    if (waitForAllAcks()) {
                        phoneTransport_.sendAck(0, 0);
                    } else {
                        phoneTransport_.sendAck(ERR_POD_TIMEOUT, 0);
                    }
                    break;

                case MSG_OTA_DATA:
                    // Relay chunk to all pods
                    for (auto& pod : podTransports_) {
                        pod->sendMessage(msg);
                    }
                    // Collect ACKs
                    if (waitForAllAcks()) {
                        phoneTransport_.sendAck(0, msg.payload.otaData.offset);
                    }
                    break;

                case MSG_OTA_END:
                    for (auto& pod : podTransports_) {
                        pod->sendMessage(msg);
                    }
                    phoneTransport_.sendAck(0, 0);
                    return;  // Session complete
            }
        }
    }

    TcpServerTransport phoneTransport_;
    std::vector<std::unique_ptr<SerialTransport>> podTransports_;
};
```

### Phone Simulator

```cpp
// host/sim/phoneSimulator.cpp
class PhoneSimulator {
public:
    PhoneSimulator(const std::string& masterHost, int masterPort)
        : transport_(masterHost, masterPort)
    {}

    int sendOta(const std::string& firmwarePath) {
        if (transport_.connect() != 0) {
            return -1;
        }

        std::vector<uint8_t> firmware = readFile(firmwarePath);

        // Same OTA flow as Phase 1, but over TCP
        sendOtaBegin(firmware);
        sendOtaChunks(firmware);
        sendOtaEnd();

        return 0;
    }

private:
    TcpClientTransport transport_;
};
```

### Usage

```bash
# Terminal 1: Run master simulator
./domes-sim master --port 8080 --pods /dev/ttyACM0,/dev/ttyACM1

# Terminal 2: Run phone simulator
./domes-sim phone --master localhost:8080 --firmware domes.bin

# Or use real phone app connecting to host IP:8080
```

---

## Phase 3: Host with Real BLE

**Goal**: Host acts as BLE central (like phone), connecting to real ESP32 master pod. From the pod's perspective, host is indistinguishable from phone app.

### Why This Matters

- Tests the ACTUAL BLE protocol, not a simulation
- Validates BLE service UUIDs, characteristics, MTU negotiation
- Can be used for automated testing without phone
- Identical bytes on the wire as production

### Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                                                                          │
│  ┌──────────────┐    BLE (BlueZ)    ┌──────────────┐   ESP-NOW          │
│  │ Host Linux   │ ────────────────► │ Real ESP32   │ ─────────►  Pods   │
│  │ BLE Central  │                   │ Master Pod   │                    │
│  │              │                   │              │                    │
│  │ Uses same    │                   │ Can't tell   │                    │
│  │ protocol as  │                   │ if it's host │                    │
│  │ phone app    │                   │ or real phone│                    │
│  └──────────────┘                   └──────────────┘                    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Prerequisites

```bash
# Install BlueZ development libraries (WSL2 needs USB passthrough)
sudo apt install libbluetooth-dev libdbus-1-dev

# For WSL2: Pass through USB Bluetooth adapter
# In PowerShell (admin):
usbipd wsl list
usbipd wsl attach --busid <bluetooth-adapter-busid>

# Verify Bluetooth works
hciconfig
# Should show: hci0: ... UP RUNNING
```

### Host BLE Transport

```cpp
// host/transport/bleHostTransport.hpp
#pragma once

#include "interfaces/iTransport.hpp"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/l2cap.h>

namespace domes::host {

/**
 * @brief BLE transport using Linux BlueZ stack
 *
 * Connects to ESP32 as BLE Central (GATT client).
 * Uses same service/characteristic UUIDs as phone app.
 */
class BleHostTransport : public ITransport {
public:
    // DOMES BLE Service UUID (must match ESP32)
    static constexpr const char* SERVICE_UUID = "12345678-1234-1234-1234-123456789abc";
    static constexpr const char* OTA_CHAR_UUID = "12345678-1234-1234-1234-123456789abd";

    BleHostTransport();
    ~BleHostTransport() override;

    /**
     * @brief Scan for DOMES devices
     * @param timeoutSec Scan duration
     * @return List of discovered device addresses
     */
    std::vector<std::string> scan(int timeoutSec = 5);

    /**
     * @brief Connect to specific device
     * @param address BLE MAC address (e.g., "AA:BB:CC:DD:EE:FF")
     */
    int connect(const std::string& address);

    // ITransport implementation
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    int receive(uint8_t* buf, size_t* len, uint32_t timeoutMs) override;
    bool isConnected() const override;
    void disconnect() override;

private:
    int discoverServices();
    int enableNotifications();

    int socket_ = -1;
    uint16_t otaCharHandle_ = 0;
    bool connected_ = false;

    // Receive buffer (BLE notifications arrive asynchronously)
    std::mutex rxMutex_;
    std::condition_variable rxCv_;
    std::queue<std::vector<uint8_t>> rxQueue_;
};

}  // namespace domes::host
```

### Host BLE Implementation (Simplified)

```cpp
// host/transport/bleHostTransport.cpp
#include "bleHostTransport.hpp"
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

// Note: Production would use D-Bus API (bluez-dbus) for better
// async support. This is simplified for illustration.

namespace domes::host {

std::vector<std::string> BleHostTransport::scan(int timeoutSec) {
    std::vector<std::string> devices;

    int dev_id = hci_get_route(nullptr);
    int sock = hci_open_dev(dev_id);

    inquiry_info* info = nullptr;
    int numDevices = hci_inquiry(dev_id, timeoutSec, 255, nullptr, &info, IREQ_CACHE_FLUSH);

    for (int i = 0; i < numDevices; i++) {
        char addr[19];
        ba2str(&info[i].bdaddr, addr);

        // Filter for DOMES devices (check name or service UUID)
        char name[248];
        if (hci_read_remote_name(sock, &info[i].bdaddr, sizeof(name), name, 0) >= 0) {
            if (strstr(name, "DOMES") != nullptr) {
                devices.push_back(addr);
                printf("Found DOMES device: %s [%s]\n", addr, name);
            }
        }
    }

    free(info);
    close(sock);

    return devices;
}

int BleHostTransport::connect(const std::string& address) {
    // Create L2CAP socket for BLE
    socket_ = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (socket_ < 0) {
        perror("socket");
        return -1;
    }

    // Set BLE-specific options
    struct bt_security sec = { .level = BT_SECURITY_LOW };
    setsockopt(socket_, SOL_BLUETOOTH, BT_SECURITY, &sec, sizeof(sec));

    // Connect
    struct sockaddr_l2 addr = {};
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_cid = htobs(4);  // ATT CID for BLE
    str2ba(address.c_str(), &addr.l2_bdaddr);
    addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;

    if (::connect(socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(socket_);
        return -1;
    }

    connected_ = true;
    printf("Connected to %s\n", address.c_str());

    // Discover GATT services and find OTA characteristic
    return discoverServices();
}

int BleHostTransport::send(const uint8_t* data, size_t len) {
    if (!connected_) return -1;

    // Build ATT Write Request
    // [opcode=0x12][handle][value...]
    std::vector<uint8_t> pdu(3 + len);
    pdu[0] = 0x12;  // Write Request
    pdu[1] = otaCharHandle_ & 0xFF;
    pdu[2] = (otaCharHandle_ >> 8) & 0xFF;
    memcpy(&pdu[3], data, len);

    ssize_t sent = ::send(socket_, pdu.data(), pdu.size(), 0);
    if (sent != pdu.size()) {
        return -1;
    }

    // Wait for Write Response
    uint8_t response[32];
    ssize_t received = recv(socket_, response, sizeof(response), 0);
    if (received < 1 || response[0] != 0x13) {  // Write Response opcode
        return -1;
    }

    return 0;
}

}  // namespace domes::host
```

### Using Host BLE for OTA

```cpp
// host/ble_ota_sender.cpp
#include "transport/bleHostTransport.hpp"
#include "protocol/ota_protocol.hpp"

int main(int argc, char* argv[]) {
    BleHostTransport ble;

    // Scan for DOMES devices
    printf("Scanning for DOMES devices...\n");
    auto devices = ble.scan(5);

    if (devices.empty()) {
        fprintf(stderr, "No DOMES devices found\n");
        return 1;
    }

    // Connect to first device (or specific one)
    printf("Connecting to %s...\n", devices[0].c_str());
    if (ble.connect(devices[0]) != 0) {
        fprintf(stderr, "Connection failed\n");
        return 1;
    }

    // Send OTA using same protocol as phone app
    std::vector<uint8_t> firmware = readFile(argv[1]);

    // OTA_BEGIN
    OtaBeginMsg begin = { .size = firmware.size() };
    computeSha256(firmware, begin.sha256);
    ble.send(serializeOtaBegin(begin));

    // OTA_DATA chunks (respecting BLE MTU)
    const size_t MTU = 244;  // Typical BLE MTU - 3 for ATT header
    for (size_t offset = 0; offset < firmware.size(); offset += MTU) {
        OtaDataMsg data = { .offset = offset };
        data.len = std::min(MTU, firmware.size() - offset);
        memcpy(data.data, firmware.data() + offset, data.len);

        ble.send(serializeOtaData(data));
        waitForAck(ble);

        printf("\rProgress: %.1f%%", 100.0 * offset / firmware.size());
    }

    // OTA_END
    ble.send(serializeOtaEnd());

    printf("\nOTA sent! Device will reboot.\n");
    return 0;
}
```

### Run BLE OTA from Host

```bash
# Build
cd firmware/host/build
cmake .. -DWITH_BLE=ON
make

# Run (requires Bluetooth adapter passed through to WSL2)
sudo ./ble_ota_sender ../domes/build/domes.bin

# Output:
# Scanning for DOMES devices...
# Found DOMES device: AA:BB:CC:DD:EE:FF [DOMES-Pod-001]
# Connecting to AA:BB:CC:DD:EE:FF...
# Connected!
# Progress: 100.0%
# OTA sent! Device will reboot.
```

---

## Implementation Roadmap

| Phase | Description | Effort | Validates |
|-------|-------------|--------|-----------|
| **1** | Serial OTA sender | 2-3 days | OTA protocol, flash writing |
| **2** | TCP master simulator | 3-4 days | Multi-pod relay, coordination |
| **3** | Host BLE transport | 4-5 days | Real BLE protocol, MTU, services |
| **4** | Full integration | 2-3 days | End-to-end with real hardware |

### Phase 1 Deliverables
- [ ] `common/protocol/ota_protocol.hpp` - Message definitions
- [ ] `common/protocol/ota_protocol.cpp` - Serialization
- [ ] `host/transport/serialTransport.cpp` - Serial port wrapper
- [ ] `host/simple_ota_sender.cpp` - CLI tool
- [ ] `domes/main/services/serialOtaReceiver.cpp` - ESP32 receiver

### Phase 2 Deliverables
- [ ] `host/transport/tcpTransport.cpp` - TCP client/server
- [ ] `host/sim/masterSimulator.cpp` - Master pod logic
- [ ] `host/sim/phoneSimulator.cpp` - Phone app logic

### Phase 3 Deliverables
- [ ] `host/transport/bleHostTransport.cpp` - BlueZ BLE
- [ ] `host/ble_ota_sender.cpp` - BLE OTA CLI
- [ ] WSL2 Bluetooth passthrough documentation

---

## Testing Strategy

### Unit Tests (Host)
```bash
# Protocol parsing tests
./test_ota_protocol

# Transport tests (loopback)
./test_serial_transport --loopback
./test_tcp_transport --loopback
```

### Integration Tests
```bash
# Phase 1: Host → Single Pod
./simple_ota_sender /dev/ttyACM0 test_firmware.bin

# Phase 2: Phone Sim → Host Master → Multiple Pods
./domes-sim master --pods /dev/ttyACM0,/dev/ttyACM1 &
./domes-sim phone --firmware test_firmware.bin

# Phase 3: Host BLE → Real Master → Real Pods
sudo ./ble_ota_sender test_firmware.bin
```

### CI Integration
```yaml
# .github/workflows/host-sim.yml
jobs:
  build-host:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build host simulator
        run: |
          cd firmware/host
          mkdir build && cd build
          cmake ..
          make
      - name: Run protocol tests
        run: ./firmware/host/build/test_ota_protocol
```

---

## Notes

### WSL2 USB Passthrough
WSL2 doesn't have native USB access. Use `usbipd-win` to pass through:
- USB-Serial adapters for ESP32 connection
- Bluetooth adapters for BLE testing

```powershell
# PowerShell (Admin)
winget install usbipd

# List devices
usbipd wsl list

# Attach USB-Serial
usbipd wsl attach --busid 1-3

# Attach Bluetooth
usbipd wsl attach --busid 1-5
```

### Alternative: Native Linux
For best experience, use native Linux (dual-boot or separate machine).
WSL2 USB passthrough can be finicky.

---

*Prerequisites: 04-communication.md, 08-ota-updates.md*
*Related: 06-testing.md (test strategy)*
