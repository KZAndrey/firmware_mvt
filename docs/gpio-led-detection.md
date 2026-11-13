# GPIO LED Detection Sensor

Only nrf52_promicro_diy_tcxo opts into MOVETAIN_DETECTION_GPIO_CHANNEL.
Detection alerts and periodic reports use the enabled gpio channel, matched case-insensitively.
A missing or disabled gpio channel prevents transmission; there is no primary-channel fallback.
Channel encryption uses the configured PSK, not PKI; this does not authenticate the sender.
Keep the Detection Sensor GPIO out of RemoteHardware available pins, because READ_GPIOS changes its input mode.
The portal matches node ID and sensor name. The packet does not contain a pin number. Only one sensor module exists per radio.
Both-edge triggers report inactivity; the minimum interval can still miss brief changes. Use periodic state reports for refresh.
On-demand reads are not implemented. Stock firmware restores primary-channel routing.

Run host tests: python tests/test_detection_gpio_channel.py <C++ compiler> [arguments].
Build target: nrf52_promicro_diy_tcxo. Keep generated firmware, local package overrides and logs out of Git.
