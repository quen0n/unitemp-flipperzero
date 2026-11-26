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
#include "unitemp_logger.h"

#include <furi_hal_rtc.h>
#include <datetime.h>
#include <string.h>

#define LOG_DIR             APP_DATA_PATH("logs")
#define LOG_HEADER          "date;time;sensor;model;temperature_C;humidity_pct;pressure_hPa;co2_ppm\r\n"
//Longest wait for every sensor to answer before the first sample of a session
#define LOG_FIRST_WAIT_MS   15000UL
//Blinking period of the indicator
#define LOG_BLINK_PERIOD_MS 1000UL

const char* const unitemp_logger_interval_text[UNITEMP_LOG_INTERVAL_COUNT] =
    {"10 s", "30 s", "1 min", "5 min", "15 min", "30 min", "1 h"};
const uint32_t unitemp_logger_interval_s[UNITEMP_LOG_INTERVAL_COUNT] =
    {10, 30, 60, 300, 900, 1800, 3600};

static struct {
    //Logging enabled for the current monitor session
    volatile bool active;
    //The last write failed
    volatile bool error;
    //The first sample of the session is still to be written
    bool first_pending;
    //Session start (ticks)
    uint32_t start_tick;
    //Interval slot of the last sample (RTC timestamp / interval)
    uint32_t last_slot;
} logger;

void unitemp_logger_start(UnitempApp* app) {
    logger.active = app->settings->logging;
    logger.error = false;
    logger.first_pending = true;
    logger.start_tick = furi_get_tick();
    logger.last_slot = 0;
}

void unitemp_logger_stop(void) {
    logger.active = false;
}

static bool sensor_has_values(Sensor* sensor) {
    return sensor->temperature != -128.0f &&
           (sensor->status == UT_SENSORSTATUS_OK || sensor->status == UT_SENSORSTATUS_POLLING);
}

//Appends value with the given number of decimals and a ',' decimal mark (no float printf)
static void append_fixed(FuriString* line, float value, uint8_t decimals) {
    int32_t scale = 1;
    for(uint8_t i = 0; i < decimals; i++)
        scale *= 10;
    float scaled = value * (float)scale;
    int32_t v = (int32_t)(scaled < 0 ? scaled - 0.5f : scaled + 0.5f);
    const char* sign = "";
    if(v < 0) {
        sign = "-";
        v = -v;
    }
    if(decimals == 0) {
        furi_string_cat_printf(line, "%s%ld", sign, v);
    } else {
        furi_string_cat_printf(
            line, "%s%ld,%0*ld", sign, v / scale, (int)decimals, v % scale);
    }
}

static void append_sensor_line(FuriString* line, const DateTime* dt, Sensor* sensor) {
    furi_string_cat_printf(
        line,
        "%04u-%02u-%02u;%02u:%02u:%02u;",
        dt->year,
        dt->month,
        dt->day,
        dt->hour,
        dt->minute,
        dt->second);
    //Sensor name, without the CSV separator
    for(const char* c = sensor->name; *c; c++) {
        furi_string_push_back(line, (*c == ';') ? ',' : *c);
    }
    furi_string_cat_printf(line, ";%s;", sensor->model->modelname);

    const SensorDataType type = sensor->model->data_type;
    append_fixed(line, sensor->temperature, 2);
    furi_string_push_back(line, ';');
    if(type == UT_DATA_TYPE_TEMP_HUM || type == UT_DATA_TYPE_TEMP_HUM_PRESS ||
       type == UT_DATA_TYPE_TEMP_HUM_CO2) {
        append_fixed(line, sensor->humidity, 1);
    }
    furi_string_push_back(line, ';');
    if(type == UT_DATA_TYPE_TEMP_PRESS || type == UT_DATA_TYPE_TEMP_HUM_PRESS) {
        append_fixed(line, sensor->pressure / 100.0f, 1);
    }
    furi_string_push_back(line, ';');
    if(type == UT_DATA_TYPE_TEMP_HUM_CO2) {
        append_fixed(line, sensor->co2, 0);
    }
    furi_string_cat_str(line, "\r\n");
}

static bool logger_write_sample(UnitempApp* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* lines = furi_string_alloc();
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        Sensor* sensor = unitemp_sensors_get(i);
        if(sensor_has_values(sensor)) append_sensor_line(lines, &dt, sensor);
    }
    if(furi_string_empty(lines)) {
        //No sensor answered: nothing to write, not an SD card error
        furi_string_free(lines);
        return true;
    }

    FuriString* path = furi_string_alloc_printf(
        "%s/%04u-%02u-%02u.csv", LOG_DIR, dt.year, dt.month, dt.day);
    storage_common_mkdir(app->storage, APP_DATA_PATH());
    storage_common_mkdir(app->storage, LOG_DIR);

    bool ok = false;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        ok = true;
        if(storage_file_size(file) == 0) {
            ok = storage_file_write(file, LOG_HEADER, strlen(LOG_HEADER)) == strlen(LOG_HEADER);
        }
        if(ok) {
            const size_t len = furi_string_size(lines);
            ok = storage_file_write(file, furi_string_get_cstr(lines), len) == len;
        }
    }
    storage_file_close(file);
    storage_file_free(file);

    if(!ok) FURI_LOG_E(APP_NAME, "Logging to %s failed", furi_string_get_cstr(path));
    furi_string_free(path);
    furi_string_free(lines);
    return ok;
}

void unitemp_logger_tick(UnitempApp* app) {
    if(!logger.active || unitemp_sensors_get_count() == 0) return;

    const uint32_t slot = furi_hal_rtc_get_timestamp() /
                          unitemp_logger_interval_s[app->settings->log_interval];

    if(logger.first_pending) {
        //First sample as soon as every sensor has answered (or after a timeout)
        bool all_ready = true;
        for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
            if(!sensor_has_values(unitemp_sensors_get(i))) all_ready = false;
        }
        if(!all_ready && furi_get_tick() - logger.start_tick < LOG_FIRST_WAIT_MS) return;
        logger.first_pending = false;
    } else if(slot == logger.last_slot) {
        //Next samples aligned on the clock: 10:05:00, 10:06:00...
        return;
    }
    logger.last_slot = slot;
    logger.error = !logger_write_sample(app);
}

void unitemp_logger_draw_indicator(Canvas* canvas, uint8_t x, uint8_t y) {
    if(!logger.active) return;
    if(logger.error) {
        canvas_draw_circle(canvas, x, y, 2);
    } else if((furi_get_tick() % LOG_BLINK_PERIOD_MS) < LOG_BLINK_PERIOD_MS / 2) {
        canvas_draw_disc(canvas, x, y, 2);
    }
}
