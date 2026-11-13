# JYNKOTECH | ESP32 Wi-Fi Repeater

![Jynkotech official logo](jynkotech-logo.png)

**Industrial Automation • Embedded Systems • IoT**  
**Website:** https://www.jynkotech.com/  
**Company email:** [jynkotech@gmail.com](mailto:jynkotech@gmail.com)  
**Maintainer:** Jynkotech

---


An open-source **ESP32 2.4 GHz Wi-Fi to Wi-Fi IPv4 NAT router** with a built-in, local web management panel. Developed by **Jynkotech**.

> **Current release: v3.1.** The author reports successful operation on their ESP32. This repository's packaged build has not been independently compiled or hardware-tested in this environment. Your results may depend on the installed Arduino-ESP32 core and the upstream router.

## Features

- Built-in web dashboard: **Status**, **Saved Networks**, **Repeater Wi-Fi**, **Network**, **Diagnostics**, **System**.
- Up to **8 saved upstream Wi-Fi networks**, ordered by priority; automatically tries another network after a connection failure.
- Wi-Fi AP SSID and password settings, hide SSID, and connected-client limit.
- Configurable gateway and DHCP-advertised DNS; NAT via the Arduino-ESP32 `WiFi.AP.enableNAPT()` API.
- Optional Wi-Fi power-saving settings and selectable transmit-power profile.
- Persistent configuration using ESP32 NVS (`Preferences`).
- Router-side connection, DNS, TCP and subnet-overlap diagnostics.
- Change admin password, restart and factory reset from the browser.
- No external Arduino libraries: uses core-bundled `Arduino`, `WiFi`, `WebServer`, and `Preferences`.

## Requirements

- Classic ESP32 Dev Module (for example ESP32-WROOM-32).
- Arduino IDE and **ESP32 by Espressif Systems**, targeted to **Arduino-ESP32 3.3.x**.
- 2.4 GHz upstream Wi-Fi, USB cable and stable USB power.

## Flashing

1. Download this repository (Code → Download ZIP) and extract it.
2. Open `Jynkotech_ESP32_Repeater_v3_1/Jynkotech_ESP32_Repeater_v3_1.ino` in Arduino IDE.
3. Install **ESP32 by Espressif Systems** in Boards Manager (3.3.x targeted).
4. Select **Tools → Board → ESP32 Dev Module**, the correct serial port, then Upload.
5. Connect your phone/PC to the default access point and open `http://192.168.50.1`.
6. Open **Saved Networks**, add your router SSID/password, and configure repeater SSID/password.
7. Change **both** the default admin password and repeater password before using the device.
8. Test an actual website from a connected client; an 'NAT enabled' status alone does **not** prove working internet.

### Initial credentials (factory defaults only)

| Setting | Initial value |
|---|---|
| Repeater SSID | `Jynkotech_Repeater` |
| Repeater WPA2 password | `ChangeMe123!` |
| Configuration URL | `http://192.168.50.1` |
| Administrator username | `admin` |
| Administrator password | `ChangeMe123!` |

These values are **public and insecure until changed**. Stored router credentials are located in device NVS flash, **not** in this repository. Never upload flash dumps or personal credential files.

## How failover works

If a saved upstream Wi-Fi connection fails, the repeater tries another saved router in priority order. The repeater SSID remains the same. This version does **not** guarantee automatic switching if the upstream Wi-Fi connection stays associated but its ISP connection fails. The diagnostic test checks ESP32 outbound connectivity; it does not prove NAT forwarding to every client.

## Restore / unhide the network

If you enabled **Hide SSID**, you may manually join the hidden network using its exact SSID and WPA2 password, then visit the portal to disable the option.

If locked out, in Arduino IDE select **Tools → Erase All Flash Before Sketch Upload → Enabled**, reflash and return that option to Disabled. **This deletes all saved Wi-Fi credentials and configuration.** Simply re-uploading without erasing flash retains settings. The BOOT-button factory reset is **not** implemented in v3.1.

## Important limitations

- This is **IPv4 NAT**, not a transparent layer-2 bridge; client devices use a distinct subnet.
- Built-in ESP32 2.4 GHz radio is shared by STA and AP, limiting throughput and range.
- Setting transmit power does **not** improve hardware receive sensitivity, and allowed power varies by region.
- Web management currently uses **unencrypted HTTP with Basic authentication**, not HTTPS. Use on trusted networks only; do not expose port 80 to the internet.
- The default administrator and AP credentials must be changed immediately.
- NAPT depends on compatible ESP32 core/network build settings; **no universal hardware/firmware compatibility guarantee**.
- If upstream and repeater use the same IPv4 subnet, choose a different repeater gateway in Network settings.

## Version history

- **v3.1** — NAT setup after upstream DHCP connection, subnet conflict warnings, DNS/TCP diagnostics.
- **v3.0** — Multiple saved upstream routers and automatic connection failover.
- **v2.0** — Extended web configuration and range/network settings.
- **v1.0** — Initial single-router access point with NAT.

## License

Released under the [MIT License](LICENSE). Copyright © 2026 Jynkotech.

Contributions and bug reports are welcome through GitHub Issues or Pull Requests. Please avoid including live Wi-Fi credentials or personal network details in issue reports.


## Company & support

Developed and maintained by **Jynkotech**, an industrial automation and embedded systems engineering company. For general business inquiries, use [jynkotech@gmail.com](mailto:jynkotech@gmail.com) or visit [www.jynkotech.com](https://www.jynkotech.com/). For technical bugs and code contributions, please prefer GitHub Issues and Pull Requests, and never share Wi-Fi passwords.
