# Security

This is a hobbyist/experimental network-router project, not a hardened commercial networking product.

- Management is **HTTP-only** and uses HTTP Basic authentication. Credentials can be exposed to anyone able to sniff or intercept local traffic.
- The device ships with **publicly documented factory credentials**. Change both passwords immediately.
- Do not forward the portal to the public internet or rely on it for an untrusted shared network.
- Do not post actual Wi-Fi passwords, NVS dumps, flash backups, browser authentication headers or private IP inventories in Issues.
- Bug reports should contain firmware and Arduino-ESP32 versions, redacted logs, expected behaviour and reproduction steps.
- For a suspected security vulnerability, do not post exploit details publicly until a maintainer has acknowledged a private disclosure channel.

For private vulnerability reports, contact **jynkotech@gmail.com** with the subject **ESP32 Repeater Security**. Please avoid sending unredacted credentials or NVS images.
