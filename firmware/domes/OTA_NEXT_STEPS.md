# OTA Implementation - Next Steps

## Current Status (Jan 2026)

The OTA system is **implemented but not end-to-end tested**. All code compiles and the device boots correctly.

### What's Working
- WiFi Manager with SmartConfig provisioning
- GitHub Releases API client
- OTA Manager with esp_https_ota
- Self-test and automatic rollback
- Version management from git tags
- CI/CD workflows for build and release

### What Needs Testing
- Full OTA update cycle (download → verify → reboot → confirm)
- WiFi connection in real environment
- Mock server integration test

## To Resume Testing

### 1. Configure WiFi

**Option A: Hardcode credentials (quick test)**

Edit `main/main.cpp`, in `app_main()` after OTA init:
```cpp
// Test WiFi connection
static domes::WifiManager wifi(configStorage);
wifi.init();
wifi.connect("YOUR_SSID", "YOUR_PASSWORD", true);

// Wait for connection
for (int i = 0; i < 30 && !wifi.isConnected(); i++) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(kTag, "Waiting for WiFi... %d", i);
}

if (wifi.isConnected()) {
    char ip[16];
    wifi.getIpAddress(ip, sizeof(ip));
    ESP_LOGI(kTag, "Connected! IP: %s", ip);
}
```

**Option B: Use SmartConfig**
1. Flash firmware
2. Download ESP-Touch app on phone
3. Enter WiFi credentials in app
4. Device receives and stores credentials

### 2. Start Mock OTA Server

On your development machine (same network as device):
```bash
cd firmware/domes/test/integration/mock_server

# Serve the current build
python server.py --firmware ../../../build/domes.bin --version v1.0.0 --port 8080
```

### 3. Configure Device to Use Mock Server

In `main/main.cpp`, modify `initOta()`:
```cpp
// Point to local mock server instead of GitHub
github.setEndpoint("http://YOUR_PC_IP:8080/repos/pcesar22/domes/releases/latest");
```

### 4. Test OTA Flow

1. Create git tag: `git tag v0.1.0`
2. Build and flash: `idf.py build flash`
3. Start mock server with v1.0.0 binary
4. Device should detect "update available"
5. Trigger update (add button handler or auto-check)
6. Verify device reboots to new version

### 5. Verify Rollback

1. Build a "bad" firmware that fails self-test
2. OTA update to bad firmware
3. Device should rollback to previous version automatically

## File Reference

| File | Purpose |
|------|---------|
| `main/interfaces/iWifiManager.hpp` | WiFi interface |
| `main/interfaces/iOtaManager.hpp` | OTA interface |
| `main/services/wifiManager.hpp/.cpp` | WiFi implementation |
| `main/services/githubClient.hpp/.cpp` | GitHub API client |
| `main/services/otaManager.hpp/.cpp` | OTA implementation |
| `test/mocks/mockWifiManager.hpp` | WiFi mock for tests |
| `test/integration/mock_server/server.py` | Mock OTA server |
| `components/certs/github_root_ca.pem` | HTTPS certificate |
| `.github/workflows/firmware-ci.yml` | CI pipeline |
| `.github/workflows/firmware-release.yml` | Release automation |

## Production Considerations

For multi-pod updates (BLE cascade), see `research/architecture/08-ota-updates.md`.
The WiFi OTA is primarily for development; production will use phone app → BLE → ESP-NOW relay.
