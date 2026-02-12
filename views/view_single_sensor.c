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
#include "view_single_sensor.h"
#include "../unitemp.h"
#include "../interfaces/singlewire.h"
#include "../interfaces/unitemp_i2c.h"

#include <stdlib.h>
#include <gui/elements.h>
#include <locale/locale.h>

#include "unitemp_icons.h"

#define TEMP_STR_SIZE 30
static char temp_str[TEMP_STR_SIZE];

struct SingleSensor {
    View* view;
    void* context;
};

typedef struct {
    uint8_t sensor_index;
    void* context;
} SingleSensorViewModel;

#define UT_DATA_POS_CENTER      37, 23
#define UT_DATA_POS_UP_LEFT     9, 16
#define UT_DATA_POS_UP_MIDDLE   37, 16
#define UT_DATA_POS_UP_RIGHT    65, 16
#define UT_DATA_POS_DOWN_LEFT   9, 39
#define UT_DATA_POS_DOWN_MIDDLE 37, 39
#define UT_DATA_POS_DOWN_RIGHT  65, 39
#define UT_DATA_POS_NONE        255, 255

//Массив содержит в себе сколько элементов в себе содержит то или иное отображение UT_DATA_TYPE
static const uint8_t data_types_values_count[UT_DATA_TYPE_COUNT] = {
    1, //UT_DATA_TYPE_TEMP
    2, //UT_DATA_TYPE_TEMP_HUM
    2, //UT_DATA_TYPE_TEMP_PRESS
    3, //UT_DATA_TYPE_TEMP_HUM_PRESS
    3, //UT_DATA_TYPE_TEMP_HUM_CO2
};
//Массив содержит координаты для отображения одного, двух и более элементов
static const uint8_t values_positions[4][4][2] = {
    {{UT_DATA_POS_CENTER}},
    {{UT_DATA_POS_UP_MIDDLE}, {UT_DATA_POS_DOWN_MIDDLE}},
    {{UT_DATA_POS_UP_LEFT}, {UT_DATA_POS_UP_RIGHT}, {UT_DATA_POS_DOWN_MIDDLE}},
    {{UT_DATA_POS_UP_LEFT},
     {UT_DATA_POS_UP_RIGHT},
     {UT_DATA_POS_DOWN_LEFT},
     {UT_DATA_POS_DOWN_RIGHT}},
};

static void _draw_temperature(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    //Не рисовать, если координаты равны UT_DATA_POS_NONE
    if(x == 255 && y == 255) return;
    //Drawing a frame
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);
    canvas_draw_rframe(canvas, x, y, 54, 19, 3);

    float temperature = sensor->temperature;
    if(temperature_unit == UT_TEMP_FAHRENHEIT) {
        temperature = locale_celsius_to_fahrenheit(temperature);
    }
    int8_t temp_dec = abs((int16_t)(temperature * 10) % 10);

    //Drawing icon
    canvas_draw_icon(
        canvas,
        x + 3,
        y + 3,
        (temperature_unit == UT_TEMP_CELSIUS ? &I_temp_C_11x14 : &I_temp_F_11x14));

    if((int16_t)temperature == -128 || sensor->status == UT_SENSORSTATUS_TIMEOUT) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, "--");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 50, y + 10 + 3, AlignRight, AlignCenter, ". -");
        return;
    }

    //Whole part of temperature
    //A crutch for displaying the sign of a number less than 0
    uint8_t offset = 0;
    if(temperature < 0 && temperature > -1) {
        temp_str[0] = '-';
        offset = 1;
    }
    snprintf((char*)(temp_str + offset), TEMP_STR_SIZE, "%d", (int16_t)temperature);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 27 + ((temperature <= -10 || temperature > 99) ? 5 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);
    //Printing the fractional part of the temperature in the range from -9 to 99 (when there are two digits in the number)
    if(temperature > -10 && temperature <= 99) {
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", temp_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
}

static void _draw_humidity(
    Canvas* canvas,
    Sensor* sensor,
    HumidityMeausureUnit hum_unit,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    //Не рисовать, если координаты равны UT_DATA_POS_NONE
    if(x == 255 && y == 255) return;
    // Drawing the frame
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);
    canvas_draw_rframe(canvas, x, y, 54, 19, 3);

    if(hum_unit == UT_HUMIDITY_RELATIVE) {
        // Drawing the icon
        canvas_draw_icon(canvas, x + 3, y + 2, &I_hum_relative_9x15);
        // Relative humidity
        snprintf(temp_str, TEMP_STR_SIZE, "%d", (uint8_t)sensor->humidity);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, temp_str);
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        // Adding '%' for relative humidity
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 4, y + 10 + 7, "%");
    } else if(hum_unit == UT_HUMIDITY_DEW_POINT) {
        float dew_point = unitemp_calculate_dew_point(sensor->temperature, sensor->humidity);

        if(temperature_unit == UT_TEMP_CELSIUS) {
            canvas_draw_icon(canvas, x + 3, y + 2, &I_hum_dewpoint_c_9x15);
        } else {
            canvas_draw_icon(canvas, x + 3, y + 2, &I_hum_dewpoint_f_9x15);
            dew_point = locale_celsius_to_fahrenheit(dew_point);
        }

        // Dewpoint with a decimal
        int humidity_dec = abs((int16_t)(dew_point * 10) % 10);
        snprintf(temp_str, TEMP_STR_SIZE, "%d", (int16_t)dew_point);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, temp_str);
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        // Printing the decimal part similar to temperature display
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", humidity_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
}

static void _draw_pressure(
    Canvas* canvas,
    Sensor* sensor,
    PressureMeasureUnit pressure_unit,
    uint8_t x,
    uint8_t y,
    bool mini) {
    //Drawing a frame
    canvas_draw_rframe(canvas, x, y, mini ? 54 : 76, 20, 3);
    canvas_draw_rframe(canvas, x, y, mini ? 54 : 76, 19, 3);

    //Drawing icon
    canvas_draw_icon(canvas, x + 3, y + 4, &I_pressure_7x13);

    float pressure = sensor->pressure;

    if(pressure_unit == UT_PRESSURE_MM_HG) {
        pressure = unitemp_convert_pa_to_mm_hg(pressure);
    } else if(pressure_unit == UT_PRESSURE_IN_HG) {
        pressure = unitemp_convert_pa_to_in_hg(pressure);
    } else if(pressure_unit == UT_PRESSURE_KPA) {
        pressure = unitemp_convert_pa_to_kpa(pressure);
    } else if(pressure_unit == UT_PRESSURE_HPA) {
        pressure = unitemp_convert_pa_to_hpa(pressure);
    }

    int16_t press_int = pressure;
    int8_t press_dec = (int16_t)(pressure * 10) % 10;

    //Whole part of the pressure
    snprintf(temp_str, TEMP_STR_SIZE, "%d", press_int);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 28 + ((press_int > 99) ? 5 : 0) - ((press_int > 999 && mini) ? 3 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);
    //Printing the fractional part of the pressure in the range from 0 to 99 (when there are two digits in the number)
    if(press_int <= 99) {
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", press_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
    if(!mini) {
        canvas_set_font(canvas, FontSecondary);
        //A unit of measurement
        if(pressure_unit == UT_PRESSURE_MM_HG) {
            canvas_draw_icon(canvas, x + 56, y + 2, &I_mm_hg_15x15);
        } else if(pressure_unit == UT_PRESSURE_IN_HG) {
            canvas_draw_icon(canvas, x + 56, y + 2, &I_in_hg_15x15);
        } else if(pressure_unit == UT_PRESSURE_KPA) {
            canvas_draw_str(canvas, x + 57, y + 13, "kPa");
        } else if(pressure_unit == UT_PRESSURE_HPA) {
            canvas_draw_str(canvas, x + 58, y + 13, "hPa");
        }
    }
}

static void _draw_heat_index(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);
    canvas_draw_rframe(canvas, x, y, 54, 19, 3);

    canvas_draw_icon(canvas, x + 3, y + 3, &I_heat_index_11x14);

    float temperature = sensor->temperature;

    float heat_index;
    if(temperature >= 26.67f) {
        temperature = locale_celsius_to_fahrenheit(sensor->temperature);
        heat_index = unitemp_calculate_heat_index(temperature, sensor->humidity);
        if(temperature_unit == UT_TEMP_CELSIUS) {
            heat_index = locale_fahrenheit_to_celsius(heat_index);
        }
    } else {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, "--");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 50, y + 10 + 3, AlignRight, AlignCenter, ". -");
        return;
    }

    int16_t heat_index_int = heat_index;
    int8_t heat_index_dec = abs((int16_t)(heat_index * 10) % 10);

    snprintf(temp_str, TEMP_STR_SIZE, "%d", heat_index_int);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 27 + ((heat_index <= -10 || heat_index > 99) ? 5 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);

    uint8_t int_len = canvas_string_width(canvas, temp_str);
    snprintf(temp_str, TEMP_STR_SIZE, ".%d", heat_index_dec);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
}

static void _draw_sensor_not_responding(Canvas* canvas, Sensor* sensor) {
    const Icon* frames[] = {
        &I_flipper_happy_60x38, &I_flipper_happy_2_60x38, &I_flipper_sad_60x38};
    canvas_draw_icon(canvas, 34, 23, frames[furi_get_tick() % 2250 / 750]);

    canvas_set_font(canvas, FontSecondary);

    if(sensor->model->interface == &SINGLEWIRE) {
        snprintf(
            temp_str,
            TEMP_STR_SIZE,
            "Sensor waiting on %s",
            ((SingleWireSensor*)sensor->instance)->data_pin->name);
    }

    if(sensor->model->interface == &unitemp_i2c) {
        snprintf(temp_str, TEMP_STR_SIZE, "Sensor waiting on SDA & SCL");
    }
    // if(sensor->model->interface == &ONE_WIRE) {
    //     snprintf(
    //         temp_str,
    //         TEMP_STR_SIZE,
    //         "Sensor waiting on %d",
    //         ((OneWireSensor*)sensor->instance)->bus->gpio->num);
    // }

    // if(sensor->model->interface == &SPI) {
    //     snprintf(temp_str, TEMP_STR_SIZE, "Waiting for module on SPI pins");
    // }
    canvas_draw_str_aligned(canvas, 65, 19, AlignCenter, AlignCenter, temp_str);
}
void single_sensor_draw_sensor(Canvas* canvas, Sensor* sensor, UnitempSettings* settings) {
    if(sensor == NULL) return;

    //Drawing a frame
    canvas_draw_rframe(canvas, 0, 0, 128, 63, 7);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 7);

    //Name stamp
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignCenter, sensor->name);
    //Underscore
    uint8_t line_len = canvas_string_width(canvas, sensor->name) + 2;
    canvas_draw_line(canvas, 64 - line_len / 2, 12, 64 + line_len / 2, 12);

    SensorDataType data_type = sensor->model->data_type;

    if(sensor->status == UT_SENSORSTATUS_OK) {
        uint8_t values_count_index = data_types_values_count[data_type] - 1;
        switch(data_type) {
        case UT_DATA_TYPE_TEMP:
            _draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            break;
        case UT_DATA_TYPE_TEMP_HUM:
            values_count_index += (settings->heat_index ? 1 : 0);
            _draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            _draw_humidity(
                canvas,
                sensor,
                settings->humidity_unit,
                settings->temperature_unit,
                values_positions[values_count_index][settings->heat_index ? 2 : 1][0],
                values_positions[values_count_index][settings->heat_index ? 2 : 1][1]);
            if(settings->heat_index) {
                _draw_heat_index(
                    canvas,
                    sensor,
                    settings->temperature_unit,
                    values_positions[values_count_index][1][0],
                    values_positions[values_count_index][1][1]);
            }
            break;
        case UT_DATA_TYPE_TEMP_PRESS:
            _draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            _draw_pressure(
                canvas,
                sensor,
                settings->pressure_unit,
                values_positions[values_count_index][1][0] - 8,
                values_positions[values_count_index][1][1],
                false);
            break;
        case UT_DATA_TYPE_TEMP_HUM_PRESS:
            values_count_index += (settings->heat_index ? 1 : 0);
            _draw_temperature(
                canvas,
                sensor,
                settings->temperature_unit,
                values_positions[values_count_index][0][0],
                values_positions[values_count_index][0][1]);
            _draw_humidity(
                canvas,
                sensor,
                settings->humidity_unit,
                settings->temperature_unit,
                values_positions[values_count_index][settings->heat_index ? 3 : 1][0],
                values_positions[values_count_index][settings->heat_index ? 3 : 1][1]);
            _draw_pressure(
                canvas,
                sensor,
                settings->pressure_unit,
                values_positions[values_count_index][2][0] - (settings->heat_index ? 0 : 8),
                values_positions[values_count_index][2][1],
                settings->heat_index);
            if(settings->heat_index) {
                _draw_heat_index(
                    canvas,
                    sensor,
                    settings->temperature_unit,
                    values_positions[values_count_index][1][0],
                    values_positions[values_count_index][1][1]);
            }
            break;
        case UT_DATA_TYPE_TEMP_HUM_CO2:
            values_count_index += (settings->heat_index ? 1 : 0);
            if(settings->heat_index) {
                _draw_heat_index(
                    canvas,
                    sensor,
                    settings->temperature_unit,
                    values_positions[values_count_index][1][0],
                    values_positions[values_count_index][1][1]);
            }
            break;
        default:
            FURI_LOG_E(APP_NAME, "Unknown data type %d", sensor->model->data_type);
        }
    } else {
        _draw_sensor_not_responding(canvas, sensor);
    }
}

static void single_sensor_draw_callback(Canvas* canvas, void* model) {
    SingleSensorViewModel* view_model = model;
    UnitempApp* app = view_model->context;
    Sensor* sensor = unitemp_sensors_get()[view_model->sensor_index];
    single_sensor_draw_sensor(canvas, sensor, app->settings);
}

static bool single_sensor_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    SingleSensor* single_sensor = context;
    UnitempApp* app = single_sensor->context;
    bool consumed = false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        scene_manager_next_scene(app->scene_manager, UnitempSceneMenu);
        consumed = true;
    }

    return consumed;
}

SingleSensor* single_sensor_alloc(void* context) {
    UnitempApp* app = context;
    SingleSensor* single_sensor = malloc(sizeof(SingleSensor));

    single_sensor->view = view_alloc();
    single_sensor->context = app;
    view_allocate_model(single_sensor->view, ViewModelTypeLockFree, sizeof(SingleSensorViewModel));

    with_view_model(
        single_sensor->view,
        SingleSensorViewModel * model,
        {
            model->sensor_index = 0;
            model->context = app;
        },
        false);

    view_set_context(single_sensor->view, single_sensor);
    view_set_draw_callback(single_sensor->view, single_sensor_draw_callback);
    view_set_input_callback(single_sensor->view, single_sensor_input_callback);

    return single_sensor;
}

void single_sensor_free(SingleSensor* single_sensor) {
    furi_assert(single_sensor);
    view_free(single_sensor->view);
    free(single_sensor);
}

View* single_sensor_get_view(SingleSensor* single_sensor) {
    furi_assert(single_sensor);
    return single_sensor->view;
}

void single_sensor_refresh_data(SingleSensor* instance) {
    furi_assert(instance);

    //Вызываем перерисовку вида псевдообновлением модели. Вызывается по таймеру каждую секнуду
    with_view_model(instance->view, SingleSensorViewModel * model, { UNUSED(model); }, true);
}
