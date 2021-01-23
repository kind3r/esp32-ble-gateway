# esp32-ble-gateway
WiFi to BLE (Bluetooth Low Energy) gateway on ESP32 using a modified version of [Noble](https://github.com/abandonware/noble)'s [WebSocket protocol](https://github.com/abandonware/noble/blob/master/ws-slave.js). The modifications consist in an added authentication layer upon connection and some extra payloads here and there. It's designed to be used with [ttlock-sdk-js](https://github.com/kind3r/ttlock-sdk-js) at least until I will find the time to document the API and implement a separate Noble binding. Note that data flows is via **standard unencrypted websocket** as the ESP can barely handle the memory requirements for BLE, WiFi and WebSocket at the same time and besides gateway is supposed to be in your LAN and the BLE traffic can be easilly sniffed over the air so there isn't really a point in encrypting all communication at this time.   

> Note: this has nothing to do with the TTLock G2 official gateway, it is basically just a GATT proxy over WiFi.

Feeling generous and want to support my work, here is [my PayPal link](https://paypal.me/kind3r).  


## What works for now
- WiFi init
- BLE init
- Websocket communication and AES 128 CBC auth
- Start/stop BLE scan
- Discover devices
- Read characteristics
- Write characteristics
- Subscribe to characteristic

## How to install

This short guide explains how to install the gateway and configure the [TTLock Home Assistant addon](https://github.com/kind3r/hass-addons) so that you can interface it with a TTLock lock. You need to compile and upload the binary yourself, there is no pre-compiled version but the process should be fairly easy even for beginers.  

### Requirements

1. [Visual Studio Code (VSCode)](https://code.visualstudio.com/) and [PlatformIO](https://platformio.org/) extension.  
2. A clone of this repository  
3. A working **ESP32-WROVER board**  
4. Some type of TTLock lock paired to a working Home Assistant installation with [TTLock Home Assistant addon](https://github.com/kind3r/hass-addons)

### Preparing the ESP32

Open the cloned repo in VSCode and PlatformIO should automatically install all the required dependencies. After this is done, you need to create a `config.cpp` file in the src folder based on the `config.cpp.example` template. Update this config file with your WiFi credentials and a **random generated 128 bit AES key** that will be used for encryption during the authentication phase. You can use [online tools](https://www.allkeysgenerator.com/Random/Security-Encryption-Key-Generator.aspx) to generate the key.  

> At the moment, the project is only configured to work on **ESP32-WROVER boards**. If you have a different board, you need to edit the `platformio.ini` file and create your own env configuration. As of this writing the code takes about 1.5Mb so I'm using the `min_spiffs.csv` partition scheme in order to be able to hopefully do OTA in the future.

Connect your ESP32 to the PC, go to PlatformIO menu (the alien head on the VSCode's left toolbar, where you have files, search, plugins etc.) then in **Project Tasks** choose **env:esp-wrover** -> **General** -> **Upload and Monitor**. This should start the build process and once it is finished the compiled result will be uploaded to the ESP32. Once the upload finishes you should start seeing some debug output, including if the WiFi connection was succesfull and the **IP address of the device** (which you need for configuring the TTLock Home Assistant addon).

### Setting up HA

Once you have the ESP runing the gateway software, go to the **TTLock Home Assistant addon** configuration options and add the following:

```yaml
gateway: noble
gateway_host: IP_ADDRESS_OF_YOUR_ESP
gateway_port: 80
gateway_key: AES_KEY_FROM_ESP_CONFIG
gateway_user: admin
gateway_pass: admin
```

> The **user and password are hardcoded** to admin/admin for the moment and so is the port. You will only need to update the **IP address of the ESP gateway** and the **AES key you generated**.  

For extra debug info, you can add the `gateway_debug: true` option to log all communication to and from the gateway in Home Assistant.  

If everything was done correctly you should not be able to use the addon using the ESP32 device as a BLE gateway.  


## Todo

- check if multiple connections to multiple devices are possible (`BLEDevice::createClient` seems to store only 1 `BLEClient`, but we could just create the client ourselfs)
- Service UUID filtering for scan and allow/disallow duplicates
- Limit total number of websocket connections (currently 4), make it configurable
- Timeout for non-authenticated connections
- Initial wifi AP based setup
- Web configuration for AES key and credentials
- Investigate unstable wifi (sometimes it connects but there is no traffic; try to ping gw during setup)
- Troubleshoot random crashes

### Random thoughts

- device discovery is always sent to all authenticated clients
- give each device a unique ID (peripheralUuid) and store ID, address and address type in a Map as it is required for connection
- conection is done based on peripheralUuid translated to address and type in the noble_api
- always stop scanning before connecting to a device
- only one client can connect to a device at a time so associate websocket with connection and cleanup on disconnect
