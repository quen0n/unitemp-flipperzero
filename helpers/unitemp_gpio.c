/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "unitemp_gpio.h"

//List of available GPIO pins with their numbers and names
#define SENSOR_PINS_COUNT (int)(sizeof(gpio_list) / sizeof(const SensorGpioPin))
static const SensorGpioPin gpio_list[] = {
    {2, "2 (A7)", &gpio_ext_pa7},
    {3, "3 (A6)", &gpio_ext_pa6},
    {4, "4 (A4)", &gpio_ext_pa4},
    {5, "5 (B3)", &gpio_ext_pb3},
    {6, "6 (B2)", &gpio_ext_pb2},
    {7, "7 (C3)", &gpio_ext_pc3},
    {10, "10 (SWC)", &gpio_swclk},
    {12, "12 (SIO)", &gpio_swdio},
    {13, "13 (TX)", &gpio_usart_tx},
    {14, "14 (RX)", &gpio_usart_rx},
    {15, "15 (C1)", &gpio_ext_pc1},
    {16, "16 (C0)", &gpio_ext_pc0},
    {17, "17 (1W)", &gpio_ibutton}};

// List of interfaces that are attached to GPIO(defined by index)
//NULL - port is free, pointer to interface - port is occupied by this interface
static const SensorConnectionInterface* gpio_interfaces_list[SENSOR_PINS_COUNT] = {0};

const SensorGpioPin* unitemp_gpio_get_from_int(uint8_t number) {
    for(uint8_t i = 0; i < SENSOR_PINS_COUNT; i++) {
        if(gpio_list[i].num == number) {
            return &gpio_list[i];
        }
    }
    return NULL;
}

uint8_t unitemp_gpio_to_index(const GpioPin* gpio) {
    if(gpio == NULL) return 255;

    for(uint8_t i = 0; i < SENSOR_PINS_COUNT; i++) {
        if(gpio_list[i].pin->pin == gpio->pin && gpio_list[i].pin->port == gpio->port) {
            return i;
        }
    }

    return 255;
}

void unitemp_gpio_lock(const SensorGpioPin* gpio, const SensorConnectionInterface* interface) {
    furi_check(gpio);
    furi_check(interface);

    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = interface;
}

void unitemp_gpio_unlock(const SensorGpioPin* gpio) {
    furi_check(gpio);

    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = NULL;
}
