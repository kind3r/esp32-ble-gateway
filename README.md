# esp32-ble-gateway
WiFi to BLE gateway on ESP32 using noble websocket protocol

### Done
- WiFi init
- BLE init
- Websocket communication and AES 128 CBC auth
- Start/stop BLE scan

### Todo

- Not happy with the way bleScan->setAdvertisedDeviceCallbacks is implemented
- Make API class static
- Move websocket creation to API's init
- Device discovery to API
- Service UUID filtering for scan and allow duplicates
- Limit total number of websocket connections
- Timeout for non-authenticated connections
- Initial wifi setup
- Investigate unstable wifi (somtimes it connects but there is no traffic; try to ping gw during setup)
