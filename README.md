# esp32-ble-gateway
WiFi to BLE gateway on ESP32 using noble websocket protocol

### Done
- WiFi init
- BLE init
- Websocket communication and AES 128 CBC auth
- Start/stop BLE scan

### Todo

- check if multiple connections to multiple devices are possible (`BLEDevice::createClient` seems to store only 1 `BLEClient`, but we could just create the client ourselfs)
- Service UUID filtering for scan and allow/disallow duplicates
- Limit total number of websocket connections (currently 4)
- Timeout for non-authenticated connections
- Initial wifi AP based setup
- Web configuration for AES key and credentials
- Investigate unstable wifi (sometimes it connects but there is no traffic; try to ping gw during setup)

### Random thoughts

- device discovery is always sent to all authenticated clients
- give each device a unique ID (peripheralUuid) and store ID, address and address type in a Map as it is required for connection
- conection is done based on peripheralUuid translated to address and type in the noble_api
- always stop scanning before connecting to a device
- only one client can connect to a device at a time so associate websocket with connection and cleanup on disconnect

