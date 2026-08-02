# ESPHome Internet Watchdog  

ESPHome Internet Watchdog is an external component that automatically recovers internet connectivity by power cycling a modem or router when connectivity is lost.

It sends real DNS queries directly to multiple public resolvers. This verifies bidirectional internet access without relying on ICMP ping or a platform-specific networking library.

The watchdog component is platform-independent and uses ESPHome's common network and socket APIs. Hardware-specific relay, button and LED configuration remains in the device YAML.

## Features

- Platform-independent network monitoring
- Configurable DNS resolvers and check domain
- Non-blocking DNS connectivity checks
- Automatic modem/router power cycling
- Configurable power-off time
- Configurable boot wait time
- Exponential reboot backoff
- Startup grace period
- Home Assistant status sensors
- Maintenance mode switch
- Manual power-cycle button
- Shelly Plug S hardware button controls
- Red maintenance-mode indicator on Shelly Plug S
- Detailed logging

## How it works

At startup, the watchdog waits for Wi-Fi and then observes the startup grace period before it begins monitoring. After a relay power cycle, it waits for the configured boot time before testing again.

```
Network available?
  |
  +-- No --> wait network_connect_timeout
  |              |
  |              +-- restart the device once (optional)
  |              +-- still unavailable --> record a failed round
  |
  +-- Yes --> Query configured public DNS resolvers
                  |
                  +-- Any valid answer --> reset failure and restart counters
                  |
                  +-- All time out/fail --> record a failed round

Failed round --> below failure_threshold --> try again later
              threshold reached --> power-cycle relay
```

After a relay power cycle, the watchdog waits for the configured power-off and boot times. Repeated relay cycles use exponential backoff up to `reboot_backoff_max`, and monitoring continues until connectivity is restored.

## Installation

Add the external component to your ESPHome configuration.

```yaml
external_components:
  - source: github://sm0vph/esphome-watchdog@main
    components: [internet_watchdog]
```

## Example configuration

```yaml
internet_watchdog:
  relay: modem_power

  dns_servers:
    - 1.1.1.1
    - 8.8.8.8
    - 9.9.9.9
  check_domain: example.com
  dns_timeout: 2s

  startup_grace_time: 2min
  power_off_time: 10s
  boot_wait_time: 90s
  check_interval: 60s
  failure_threshold: 2
  network_connect_timeout: 7min
  device_reboot_before_power_cycle: true

  reboot_backoff: true
  reboot_backoff_initial: 30s
  reboot_backoff_multiplier: 2
  reboot_backoff_max: 10min

  

  maintenance_switch: modem_maintenance
  maintenance_timeout: 1h
```

## Configuration

| Option | Description |
|---------|-------------|
| `relay` | Relay controlling modem/router power |
| `dns_servers` | Public DNS resolver IP addresses queried in order (default: Cloudflare, Google and Quad9) |
| `check_domain` | Domain queried for an A record (default: `example.com`) |
| `dns_timeout` | Maximum time to wait for each DNS server (default: `2s`) |
| `startup_grace_time` | Ignore failures after boot |
| `power_off_time` | How long relay remains off |
| `boot_wait_time` | Time to wait after power is restored |
| `check_interval` | Time between complete DNS monitoring cycles (default: `60s`) |
| `failure_threshold` | Number of consecutive failed monitoring cycles required before a power cycle (default: `2`) |
| `network_connect_timeout` | How long to wait for the configured network connection before recording a failed cycle (default: `7min`) |
| `device_reboot_before_power_cycle` | Restart the ESPHome device once after a network timeout before power cycling the relay (default: `true`) |
| `reboot_backoff` | Enable exponential backoff |
| `reboot_backoff_initial` | Initial backoff delay |
| `reboot_backoff_multiplier` | Backoff multiplier |
| `reboot_backoff_max` | Maximum backoff delay |
| `maintenance_switch` | Home Assistant switch disabling automatic restarts |
| `maintenance_timeout` | Optional maximum duration for maintenance mode. When the timeout expires, normal operation resumes automatically. If omitted, maintenance mode remains enabled until manually disabled. |

## Migrating from the AsyncPing version

Remove the `ESP8266WiFi` and `AsyncPing` entries from `esphome.libraries`, then update these options:

| Previous option | Replacement |
|-----------------|-------------|
| `hosts` | `dns_servers` |
| `ping_interval` | `check_interval` |
| `wifi_connect_timeout` | `network_connect_timeout` |
| `wifi_reboot_before_power_cycle` | `device_reboot_before_power_cycle` |

The previous `gateway` option is no longer used. Add `check_domain` and `dns_timeout` only if their defaults are not suitable.

## Home Assistant entities

The component exposes:

- Internet status
- Watchdog status
- Current target
- Last failure
- Latency sensor

Optional:

- Maintenance mode switch


## Maintenance mode

When maintenance mode is enabled:

- Automatic restarts are disabled.
- Connectivity monitoring continues.
- Status information continues to update.
- If `maintenance_timeout` is configured, normal operation resumes automatically when the timeout expires.

## Shelly Plug S controls

The included `example-shelly-plug-s.yaml` configures the physical button and LEDs on a Shelly Plug S Gen1:

- A short press (50–999 ms) toggles maintenance mode.
- A long press (1–10 seconds) power cycles the connected modem or router when the button is released.
- The red LED (GPIO0) is on while maintenance mode is enabled.
- The blue LED (GPIO2) remains the ESPHome status LED.

The `Restart modem` button exposed to Home Assistant uses the same power-cycle script as a long press. The relay is no longer toggled directly by the physical button.


## Backoff

When enabled, repeated restart attempts are delayed using exponential backoff.

Example:

| Restart | Delay |
|---------:|------:|
| 1 | No delay |
| 2 | 30 seconds |
| 3 | 1 minute |
| 4 | 2 minutes |
| 5 | 4 minutes |
| 6+ | 10 minutes (maximum) |

The backoff counter is automatically reset once internet connectivity has been restored.

## Logging

Typical log output:

```
Querying example.com via DNS server 1.1.1.1
Internet OK via DNS server 1.1.1.1

Connection lost

Power cycling modem...

Waiting for boot...

Backoff expired

Resuming monitoring
```

## License

MIT License
