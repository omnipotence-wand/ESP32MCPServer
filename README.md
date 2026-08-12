# ESP32 Air Conditioner MCP Simulator

An ESP32-S3 air-conditioner simulator controlled through MCP. BLE is used for
Wi-Fi provisioning; the full AC control surface is exposed by the HTTP MCP
transport after the device joins Wi-Fi.

## HTTP MCP tools

Every tool declares both an `inputSchema` and an `outputSchema`. State-changing
tools return a compact `{ success, running }` acknowledgement to keep the
embedded schema registry within the ESP32 heap budget. `getStatus` returns the
complete device snapshot.

| Tool | Purpose |
| --- | --- |
| `turnOn`, `turnOff` | Power control |
| `setMode` | Auto, cool, heat, dehumidify, or fan-only mode |
| `setTemperature` | Set the 16-30°C target |
| `setFanSpeed` | Auto, low, medium, high, turbo, or quiet fan |
| `setSwing` | Independently control vertical and horizontal swing |
| `setAirDirection` | Set fixed five-position vertical/horizontal louvers |
| `setFeature` | Sleep, ECO, turbo, quiet, light, beep, lock, anti-direct-blow, auxiliary heat, mildew-proof, or self-clean |
| `setTimer`, `cancelTimer` | Delayed power-on and power-off timers |
| `setEnvironment` | Set simulated room temperature and humidity |
| `injectFault`, `clearFault` | Simulate and clear device faults |
| `resetFilter` | Reset simulated filter life |
| `reset` | Restore default AC settings |
| `getStatus` | Read the complete device, environment, timer, operation, and maintenance state |
| `get_description` | Read device identity metadata |

The LCD keeps the important state on one stable, high-contrast screen: mode,
power, target and room conditions, fan speed, compressor state, and estimated
power. Less important convenience features use a compact pictogram ribbon;
enabled icons light up in cyan. MCP changes briefly outline the relevant area
or the exact feature icon,
while timers, filter warnings, faults, and Wi-Fi remain visible in context.
