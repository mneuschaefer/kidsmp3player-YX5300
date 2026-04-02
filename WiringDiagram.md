# Simple Wiring Diagram

This wiring overview is based on the pin assignments used in `kidsmp3player-test.ino`.

## Arduino Pin Mapping

| Arduino Pin | Connected Part | Purpose |
| --- | --- | --- |
| `D5` | `YX5300 TX` | Serial data from MP3 module to Arduino |
| `D6` | `YX5300 RX` | Serial data from Arduino to MP3 module |
| `D3` | Status LED (+ resistor) | Playback status LED |
| `A3` | Button resistor ladder output | Reads button presses |
| `A5` | Volume potentiometer wiper | Reads volume level |
| `A0` | Optional internal volume input | Defined in code, currently not used for volume logic |
| `5V` | YX5300 `VCC`, button ladder, potentiometer, LED circuit | Power |
| `GND` | YX5300 `GND`, buttons, potentiometer, LED circuit | Ground |

## Main Connections

### YX5300 MP3 Module

| YX5300 Pin | Arduino Pin |
| --- | --- |
| `TX` | `D5` |
| `RX` | `D6` |
| `VCC` | `5V` |
| `GND` | `GND` |

Recommended:
- If your YX5300 board is sensitive on `RX`, add a simple voltage divider between Arduino `D6` and YX5300 `RX`.
- Connect the speaker to the YX5300 module, not directly to the Arduino.

## Status LED

| LED Side | Connection |
| --- | --- |
| Anode (+) | `D3` through a resistor |
| Cathode (-) | `GND` |

Suggested resistor: `220Ω` to `1kΩ`

## Volume Potentiometer

Use a standard 3-pin potentiometer.

| Potentiometer Pin | Connection |
| --- | --- |
| One outer pin | `5V` |
| Other outer pin | `GND` |
| Middle pin (wiper) | `A5` |

## Buttons

The sketch reads many buttons through a single analog pin:

- `A3` is used as the button input
- the button values are distinguished by different analog voltage levels
- this usually means a resistor ladder button board is connected to `A3`

### Button Input

| Connection | Arduino Pin |
| --- | --- |
| Button ladder output | `A3` |
| Common button circuit power | `5V` / `GND` |

The code expects these button IDs from the analog ladder:

- `1`
- `2`
- `3`
- `4`
- `5`
- `6`
- `7`
- `8`
- `9`
- `10`
- `11`

Behavior in the sketch:

- `1` to `9`: select folder / play content
- `10`: pause / resume
- `11`: restart current folder from track 1

## Simple Diagram

```text
Arduino                     YX5300 MP3 Module
-------                     -----------------
5V      ------------------> VCC
GND     ------------------> GND
D5      <------------------ TX
D6      ------------------> RX

Arduino                     LED
-------                     ---
D3      ------------------> Resistor --> LED Anode
GND     --------------------------------> LED Cathode

Arduino                     Volume Pot
-------                     ----------
5V      ------------------> Outer pin
GND     ------------------> Outer pin
A5      ------------------> Middle pin

Arduino                     Button Ladder
-------                     -------------
A3      <------------------ Analog output
5V/GND  ------------------> Button resistor network
