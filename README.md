# Bluetooth LE Watch

[![Language](https://img.shields.io/badge/Made%20with-C-blue.svg)](https://shields.io/) <img src="https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white" height='20px'/> [![build](https://github.com/CharlesDias/BLE-Watch/actions/workflows/build.yml/badge.svg)](https://github.com/CharlesDias/BLE-Watch/actions/workflows/build.yml)

This is a sample project using the Zephyr RTOS and Nordic nRF Connect to create a BLE watch. This device is composed with:

* [nRF52840 DK](https://www.nordicsemi.com/Products/Development-hardware/nRF5340-DK) Nordic board.
* OLED monochrome displays SSD1306 128x64 pixels.
* and the DS3231 real-time clock (RTC).

Both display and RTC communicates with the nRF52840 MCU via I2C bus.

Some topics covered:

* Tested on [nRF52840 DK](https://www.nordicsemi.com/Products/Development-hardware/nRF5340-DK) board.
* Embedded system with [Zephyr RTOS](https://zephyrproject.org/) and [nRF Connect SDK](https://www.nordicsemi.com/Products/Development-software/nrf-connect-sdk).
* Bluetooth Low Energy (LE) technology.
* The device works as GAP Peripheral and GATT server.
* Implemented BLE services officially adopted by Bluetooth SIG and custom services too.

## Bluetooth Services

### Device information service (dis)

Exposes the device information.

* Device Information Service: <UUID: 0x180A>
  * Characteristic: Model number string <UUID: 0x2A24>
    * Data format: < string >
    * Properties: Read.
  * Characteristic: Manufacturer name string <UUID: 0x2A29>
    * Data format: < string >
    * Properties: Read.
  * Characteristic: Serial number string <UUID: 0x2A25>
    * Data format: < string >
    * Properties: Read.
  * Characteristic: Firmware revision string <UUID: 0x2A26>
    * Data format: < string >
    * Properties: Read.
  * Characteristic: Hardware revision string <UUID: 0x2A27>
    * Data format: < string >
    * Properties: Read.
  * Characteristic: Software revision string <UUID: 0x2A28>
    * Data format: < string >
    * Properties: Read.

### Battery service (bas)

Simulates the battery level voltage.

* Battery Service: <UUID: 0x180F>
  * Characteristic: Battery level <UUID: 0x2A19>
    * Data format: < UINT8[1 byte]> percentage battery level
    * Properties: Notify, Read.

### Custom service

Receives messages to be shown on the display screen.

* Unknown Service: <UUID: 3C134D60-E275-406D-B6B4-BF0CC712CB7C>
  * Characteristic: Unknown <UUID: 3C134D61-E275-406D-B6B4-BF0CC712CB7C>
    * Data format: < TEXT (UTF-8) > limit up to 31 characters
    * Properties: Read, Write.

## Project Structure

Under development.

## Building and Running using Docker image

### Interactive usage

Access the project folder.

```console
cd ble_watch
```

And run the docker image.

```console
docker run --rm -it -v ${PWD}:/workdir/project -w /workdir/project charlesdias/nrfconnect-sdk /bin/bash
```

After that, run the command below to build the firmware.

```console
make build
```
