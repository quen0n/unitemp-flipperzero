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
#ifndef UNITEMP_LOGGER_H_
#define UNITEMP_LOGGER_H_

#include "../unitemp.h"

/*
 * Data logger: while the monitor screen is open and logging is enabled, the values of every
 * sensor are appended to a CSV file on the SD card, one file per day:
 *   /ext/apps_data/temp_sensors_reader/logs/YYYY-MM-DD.csv
 * Fixed units (degC, %, hPa, ppm), ';' separator and ',' decimal mark (spreadsheet friendly).
 * The file is opened and closed for each sample, so a power loss costs at most one sample.
 */

//Interval labels and values (seconds), selectable in the settings
extern const char* const unitemp_logger_interval_text[UNITEMP_LOG_INTERVAL_COUNT];
extern const uint32_t unitemp_logger_interval_s[UNITEMP_LOG_INTERVAL_COUNT];

/**
 * @brief Prepares a logging session (call when the monitor starts, before the reader thread)
 */
void unitemp_logger_start(UnitempApp* app);

/**
 * @brief Ends the logging session (call when the monitor stops, after the reader thread)
 */
void unitemp_logger_stop(void);

/**
 * @brief Writes a sample when one is due. Called by the reader thread after each sensors poll
 */
void unitemp_logger_tick(UnitempApp* app);

/**
 * @brief Draws the logging indicator centered on x, y (nothing when logging is off):
 * blinking dot = recording, steady empty circle = the last write failed (SD card)
 */
void unitemp_logger_draw_indicator(Canvas* canvas, uint8_t x, uint8_t y);

#endif
