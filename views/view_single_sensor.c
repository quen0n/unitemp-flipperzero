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

#include <stdlib.h>
#include <gui/elements.h>

#include "unitemp_icons.h"

#define TEMP_STR_SIZE 11
static char temp_str[TEMP_STR_SIZE];

struct SingleSensor {
    View* view;
    void* context;
};

typedef struct {
    uint8_t sensor_index;
    TempMeasureUnit unit;
    void* context;
} SingleSensorViewModel;

static void _draw_temperature(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit unit,
    uint8_t x,
    uint8_t y,
    Color color) {
    //Drawing a frame
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);

    if(color == ColorBlack) {
        canvas_draw_rbox(canvas, x, y, 54, 19, 3);
        canvas_invert_color(canvas);
    } else {
        canvas_draw_rframe(canvas, x, y, 54, 19, 3);
    }

    int8_t temp_dec = abs((int16_t)(sensor->temp * 10) % 10);

    //Drawing icon
    canvas_draw_icon(
        canvas, x + 3, y + 3, (unit == UT_TEMP_CELSIUS ? &I_temp_C_11x14 : &I_temp_F_11x14));

    if((int16_t)sensor->temp == -128 || sensor->status == UT_SENSORSTATUS_TIMEOUT) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, "--");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 50, y + 10 + 3, AlignRight, AlignCenter, ". -");
        if(color == ColorBlack) canvas_invert_color(canvas);
        return;
    }

    //Whole part of temperature
    //A crutch for displaying the sign of a number less than 0
    uint8_t offset = 0;
    if(sensor->temp < 0 && sensor->temp > -1) {
        temp_str[0] = '-';
        offset = 1;
    }
    snprintf((char*)(temp_str + offset), TEMP_STR_SIZE, "%d", (int16_t)sensor->temp);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 27 + ((sensor->temp <= -10 || sensor->temp > 99) ? 5 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);
    //Printing the fractional part of the temperature in the range from -9 to 99 (when there are two digits in the number)
    if(sensor->temp > -10 && sensor->temp <= 99) {
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", temp_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
    if(color == ColorBlack) canvas_invert_color(canvas);
}

static void single_sensor_draw_callback(Canvas* canvas, void* model) {
    SingleSensorViewModel* view_model = model;
    Sensor* sensor = unitemp_sensors_get()[0];
    _draw_temperature(canvas, sensor, view_model->unit, 37, 22, ColorWhite);
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
            model->unit = app->settings->temp_unit;
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
