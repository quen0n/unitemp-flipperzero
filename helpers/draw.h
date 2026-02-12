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
#ifndef DRAW_H_
#define DRAW_H_
#include "../unitemp.h"

#include <gui/elements.h>

/**
 * @brief Draws temperature reading on canvas
 * 
 * Renders the temperature value from a sensor onto the specified canvas at the given coordinates.
 * The temperature is formatted according to the specified measurement unit.
 * 
 * @param canvas Pointer to the Canvas object where the temperature will be drawn
 * @param sensor Pointer to the Sensor object containing temperature reading data
 * @param temperature_unit The unit in which temperature should be displayed (Celsius, Fahrenheit, etc.)
 * @param x Horizontal coordinate (column) on canvas where drawing starts
 * @param y Vertical coordinate (row) on canvas where drawing starts
 * 
 * @return void
 */
void unitemp_draw_temperature(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y);
/**
 * @brief Draws a single sensor reading on the canvas
 * 
 * Renders a sensor's current temperature and status information at the specified
 * position on the canvas display. The temperature is formatted according to the
 * specified measurement unit.
 * 
 * @param canvas Pointer to the Canvas object where the sensor data will be drawn
 * @param sensor Pointer to the Sensor object containing the sensor data to display
 * @param temperature_unit The temperature unit to use for displaying the reading (Celsius, Fahrenheit, etc.)
 * @param x The x-coordinate (horizontal position) where to draw the sensor information
 * @param y The y-coordinate (vertical position) where to draw the sensor information
 * 
 * @return void
 */
void unitemp_draw_sensor_single(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y);
#endif //DRAW_H_
