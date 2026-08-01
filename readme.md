# ESPHome Internet Watchdog  

ESPHome Internet Watchdog is an external component that automatically recovers internet connectivity by power cycling a modem or router when connectivity is lost.

It continuously verifies that the local gateway is reachable before testing one or more internet hosts, helping distinguish between local network and upstream connectivity failures.

Designed for ESP8266 devices such as the Shelly Plug S Gen1, but should work with any ESPHome-supported device controlling a relay.

## Features

- Monitor gateway and internet connectivity
- Configurable list of internet hosts
- Automatic modem/router power cycling
- Configurable power-off time
- Configurable boot wait time
- Exponential reboot backoff
- Startup grace period
- Home Assistant status sensors
- Maintenance mode switch
- Manual restart button
- Detailed logging

## How it works

At startup, the watchdog waits for Wi-Fi and then observes the startup grace period before it begins monitoring. After a relay power cycle, it waits for the configured boot time before testing again.

```
Wi-Fi available?
  |
  +-- No --> wait wifi_connect_timeout
  |              |
  |              +-- restart the ESP once (optional)
  |              +-- still unavailable --> record a failed round
  |
  +-- Yes --> Ping gateway
                  |
                  +-- Failed --> record a failed round
                  |
                  +-- OK --> Ping internet host(s)
                               |
                               +-- Any success --> reset failure and restart counters
                               |
                               +-- All fail --> record a failed round

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

  gateway: 192.168.1.1

  hosts:
    - 1.1.1.1
    - 8.8.8.8

  startup_grace_time: 2min
  power_off_time: 10s
  boot_wait_time: 90s
  ping_interval: 60s
  failure_threshold: 2
  wifi_connect_timeout: 7min
  wifi_reboot_before_power_cycle: true

  reboot_backoff: true
  reboot_backoff_initial: 30s
  reboot_backoff_multiplier: 2
  reboot_backoff_max: 10min

  

  maintenance_switch: modem_maintenance
```

## Configuration

| Option | Description |
|---------|-------------|
| `relay` | Relay controlling modem/router power |
| `gateway` | Gateway IP address |
| `hosts` | Internet hosts to test |
| `startup_grace_time` | Ignore failures after boot |
| `power_off_time` | How long relay remains off |
| `boot_wait_time` | Time to wait after power is restored |
| `ping_interval` | Time between complete monitoring cycles. Each cycle checks the gateway first, then one or more Internet hosts if the gateway is reachable. |
| `failure_threshold` | Number of consecutive failed monitoring cycles required before a power cycle (default: `2`) |
| `wifi_connect_timeout` | How long to wait for Wi-Fi after startup or power restoration before recording a failed monitoring cycle (default: `7min`) |
| `wifi_reboot_before_power_cycle` | Restart the ESP once after a Wi-Fi timeout before treating it as a router failure (default: `true`) |
| `reboot_backoff` | Enable exponential backoff |
| `reboot_backoff_initial` | Initial backoff delay |
| `reboot_backoff_multiplier` | Backoff multiplier |
| `reboot_backoff_max` | Maximum backoff delay |
| `maintenance_switch` | Home Assistant switch disabling automatic restarts |

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
Pinging gateway...
Gateway OK

Pinging host #0 (1.1.1.1)...
Internet OK

Connection lost

Power cycling modem...

Waiting for boot...

Backoff expired

Resuming monitoring
```

## License

MIT License
