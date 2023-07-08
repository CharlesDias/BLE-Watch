# Bluetooth LE Watch

[![Language](https://img.shields.io/badge/Made%20with-C-blue.svg)](https://shields.io/) <img src="https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake&logoColor=white" height='20px'/> [![build](https://github.com/CharlesDias/BLE-Watch/actions/workflows/build.yml/badge.svg)](https://github.com/CharlesDias/BLE-Watch/actions/workflows/build.yml) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=CharlesDias_BLE-Watch&metric=alert_status)](https://sonarcloud.io/dashboard?id=CharlesDias_BLE-Watch)


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
* Continuous integration (CI) with GitHub Actions, as SonarCloud integration.
* Use of Docker container.

<img src="docs/images/project.gif" alt="drawing" width="600"/>

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

```text
.
├── app
│   ├── inc
│   │   ├── device_information_service.h
│   │   ├── display_ssd1306.h
│   │   ├── gatt_central.h
│   │   └── rtc_ds3231.h
│   └── src
│       ├── device_information_service.c
│       ├── display_ssd1306.c
│       ├── gatt_central.c
│       └── rtc_ds3231.c
├── build_nrf52840dk
├── CMakeLists.txt
├── docs
│   ├── Assigned Numbers.pdf
│   ├── DS3231.pdf
│   └── DTS_v1.0.pdf
├── Kconfig
├── Makefile
├── nrf52840dk_nrf52840.overlay
├── prj.conf
├── README.md
├── sample.yaml
└── src
    └── main.c
```

## Building and running

### Building with Docker image on interactive mode

Access the project folder.

```console
cd BLE-Watch
```

And run the docker image.

```console
docker run --rm -it -v ${PWD}:/workdir/project -w /workdir/project charlesdias/nrfconnect-sdk /bin/bash
```

After that, run the command below to build the firmware.

```console
make build
```

### Running

Test the BLE Watch application with the nRF Connect app, which is available for iOS (App Store) and Android (Google Play).

![BLE Watch](docs/images/ble_watch.gif)

## Next improvements

* Create dedicate task to update the display.
* Implement Bluetooth service to set the date and time.
* Flash the application project running west on Docker container.
* Add Doxygen configuration.
