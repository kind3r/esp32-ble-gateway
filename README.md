# esp32-ble-gateway
WiFi to BLE gateway on ESP32 using noble websocket protocol

### Todo

- Move websocket messages processing into separate class
- Enable BLE and try scanning
- Limit total number of websocket connections
- Timeout for non-authenticated connections
- Initial wifi setup
- Investigate unstable wifi (somtimes it connects but there is no traffic; try to ping gw during setup)
