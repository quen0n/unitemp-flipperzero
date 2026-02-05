![Flipper usage](https://user-images.githubusercontent.com/10090793/211182642-e41919c5-3091-4125-815a-2d6a77a859f6.png)
# Unitemp - Universal temperature sensor reader
[![GitHub release](https://img.shields.io/github/release/quen0n/unitemp-flipperzero?include_prereleases=&sort=semver&color=blue)](https://github.com/quen0n/unitemp-flipperzero/releases/)
[![License](https://img.shields.io/github/license/quen0n/unitemp-flipperzero)](https://github.com/quen0n/unitemp-flipperzero/blob/dev/LICENSE.md)
[![Build dev](https://github.com/quen0n/unitemp-flipperzero/actions/workflows/build_dev.yml/badge.svg?branch=dev)](https://github.com/quen0n/unitemp-flipperzero/actions/workflows/build_dev.yml)

[Flipper Zero](https://flipper.net/products/flipper-zero) application for reading temperature, humidity and pressure sensors like a DHT11/22, DS18B20, BMP280, HTU21 and more. 

## 🚧 Unitemp 2 is under construction 🚧
Development of the Unitemp app began in 2022. Much has changed since then: the Flipper Zero firmware API has stabilized, and new features and improvements have been added. The new version of Unitemp will update the code to reflect these changes, and accumulated bugs will be fixed. It is also planned to add a detailed description of connecting sensors to the Flipper Zero.

## List of supported sensors
| Model          | Interface    | Temperature range           | Humidity range       |              |
|:---------------|:------------:|:----------------------------|:---------------------|:------------:|
| DHT11 (AOSONG) | Single Wire  | 0...50°C (±2°C, 1°C)        | 20...90% (±5%, 1%)   | [<img src=".github\images\sensors\DHT11\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT11-module.html) |
| DHT11 (ASAIR)  | Single Wire  | -20...60°C (±2°C, 0.1°C)    | 5...95% (±5%, 1%)    | [<img src=".github\images\sensors\DHT11\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT11-module.html) |
| DHT12          | Single Wire  | -20...60°C (±0.5°C, 0.1°C)  | 20...90% (±5%, 0.1%) | [<img src=".github\images\sensors\DHT12\general.png" width="150"/>](https://www.aliexpress.com/w/wholesale-DHT12-module.html) |

## Gratitudes
- [Svaarich](https://github.com/Svaarich) for the UI design 
- [Unleashed firmware](https://github.com/DarkFlippers/unleashed-firmware) community for sensors testing and feedbacks
- ...and other people who send pull requests with new sensors and fixes
