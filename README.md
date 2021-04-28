# esp32-ble-gateway
WiFi to BLE (Bluetooth Low Energy) gateway on ESP32 using a modified version of [Noble](https://github.com/abandonware/noble)'s [WebSocket protocol](https://github.com/abandonware/noble/blob/master/ws-slave.js). The modifications consist in an added authentication layer upon connection and some extra payloads here and there. It's designed to be used with [ttlock-sdk-js](https://github.com/kind3r/ttlock-sdk-js) at least until I will find the time to document the API and implement a separate Noble binding. Note that data flows is via **standard unencrypted websocket** as the ESP can barely handle the memory requirements for BLE, WiFi and WebSocket at the same time and besides gateway is supposed to be in your LAN and the BLE traffic can be easilly sniffed over the air so there isn't really a point in encrypting all communication at this time.   

> Note: this has nothing to do with the TTLock G2 official gateway, it is basically just a GATT proxy over WiFi.

Feeling generous and want to support my work, here is [my PayPal link](https://paypal.me/kind3r).  


## What works for now
- WiFi init with AP style configuration via HTTPS web page
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

Open the cloned repo in VSCode and PlatformIO should automatically install all the required dependencies (it will take a couple of minutes, depending on your computer and internet speed, be patient and let it *settle*). You need to modify `sdkconfig.h` located in `.platformio/packages/framework-arduinoespressif32/tools/sdk/include/config` and change `CONFIG_ARDUINO_LOOP_STACK_SIZE` to `10240`. This is because the HTTPS certificate generation takes more stack space.

> At the moment, the project is only configured to work on **ESP32-WROVER boards**. If you have a different board, you need to edit the `platformio.ini` file and create your own env configuration. As of this writing the code takes about 1.5Mb so I'm using the `min_spiffs.csv` partition scheme in order to be able to hopefully do OTA in the future.

Connect your ESP32 to the PC, go to PlatformIO menu (the alien head on the VSCode's left toolbar, where you have files, search, plugins etc.) then in **Project Tasks** choose **env:esp-wrover** -> **Platform** -> **Upload Filesystem Imager**. This will 'format' the storage and upload the web UI.  

Next, you need to build and upload the main code. In **Project Tasks** choose **env:esp-wrover** -> **General** -> **Upload and Monitor**. This should start the build process and once it is finished the compiled result will be uploaded to the ESP32.

Once the upload finishes you should start seeing some debug output, including the status of the WiFi AP and HTTPS certificate generation status (it will take quite some time so be patient). After the startup is completed, you can connect to ESP's AP named **ESP32GW** with password **87654321** and access [https://esp32gw.local](https://esp32gw.local). The browser will complain about the self-signed certificate but you can ignore and continue. The default username and password are **admin/admin**. Configure your wifi credentials and copy the **AES Key** which you need to setup in the **TTLock Home Assistant addon**.  

After saving the new configuration, the ESP will reboot, connect to your WiFi and output it's **IP address** on the serial port (it will also generate a new HTTPS certificate if you changed it's name). It will also be accessible via `esp32gw.local` (or the new name you gave it) via MDNS if this service is working in your network. You can still make configuration changes by accessing it's IP address in the browser.

### Setting up HA

Once you have the ESP running the gateway software, go to the **TTLock Home Assistant addon** configuration options and add the following:

```yaml
gateway: noble
gateway_host: IP_ADDRESS_OF_YOUR_ESP
gateway_port: 8080
gateway_key: AES_KEY_FROM_ESP_CONFIG
gateway_user: admin
gateway_pass: admin
```

> The **user and password are hardcoded** to admin/admin for the moment and so is the port. You will only need to update the **IP address of the ESP gateway** and the **AES key you generated**.  

For extra debug info, you can add the `gateway_debug: true` option to log all communication to and from the gateway in Home Assistant.  

If everything was done correctly you should now be able to use the addon using the ESP32 device as a BLE gateway.  

## Todo

- check if multiple connections to multiple devices are possible (`BLEDevice::createClient` seems to store only 1 `BLEClient`, but we could just create the client ourselves)
- Service UUID filtering for scan and allow/disallow duplicates
- Timeout for non-authenticated connections
- Investigate unstable wifi (sometimes it connects but there is no traffic; try to ping gw during setup)
- Optimize memory fragmentation

### Random thoughts

- device discovery is always sent to all authenticated clients
- give each device a unique ID (peripheralUuid) and store ID, address and address type in a Map as it is required for connection
- connection is done based on peripheralUuid translated to address and type in the noble_api
- always stop scanning before connecting to a device
- only one client can connect to a device at a time so associate websocket with connection and cleanup on disconnect
