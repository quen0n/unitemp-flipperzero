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
#include "view_many_sensors.h"
#include "../unitemp.h"
#include "../helpers/draw.h"

#include <stdlib.h>
#include <gui/elements.h>
#include <locale/locale.h>

extern const Icon I_ButtonRight_4x7;
extern const Icon I_ButtonLeft_4x7;

struct ManySensors {
    View* view;
    void* context;
};

typedef struct {
    uint8_t sensors_page;
    void* context;
} ManySensorsViewModel;

void many_sensors_draw(Canvas* canvas, ManySensorsViewModel* view_model) {
    if(view_model == NULL || canvas == NULL) return;
    UnitempSettings* settings = ((UnitempApp*)(view_model->context))->settings;

    uint8_t pages = unitemp_sensors_get_count() / 4 + (unitemp_sensors_get_count() % 4 ? 1 : 0);
    if(view_model->sensors_page >= pages) {
        view_model->sensors_page = 0;
    }

    //Number of sensors that will be displayed on the page
    uint8_t page_sensors_count;
    if((unitemp_sensors_get_count() - view_model->sensors_page * 4) / 4) {
        page_sensors_count = 4;
    } else {
        page_sensors_count = (unitemp_sensors_get_count() - view_model->sensors_page * 4) % 4;
    }

    //Drawing a frame
    canvas_draw_rframe(canvas, 0, 0, 128, 63, 7);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 7);

    //Right arrow
    if(pages > 0 && view_model->sensors_page < pages - 1) {
        canvas_draw_icon(canvas, 122, 29, &I_ButtonRight_4x7);
    }
    //Left arrow
    if(view_model->sensors_page > 0) {
        canvas_draw_icon(canvas, 2, 29, &I_ButtonLeft_4x7);
    }

    const uint8_t value_positions[][4][2] = {
        {{36, 18}}, //1 sensor
        {{7, 18}, {67, 18}}, //2 sensors
        {{7, 3}, {67, 3}, {37, 33}}, //3 sensors
        {{7, 3}, {67, 3}, {7, 33}, {67, 33}}}; //4 sensors
    //Drawing a frame
    canvas_draw_rframe(canvas, 0, 0, 128, 63, 7);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 7);
    for(uint8_t i = 0; i < page_sensors_count; i++) {
        unitemp_draw_sensor_single(
            canvas,
            unitemp_sensors_get()[view_model->sensors_page * 4 + i],
            settings->temperature_unit,
            value_positions[page_sensors_count - 1][i][0],
            value_positions[page_sensors_count - 1][i][1]);
    }
}

static void many_sensors_draw_callback(Canvas* canvas, void* model) {
    ManySensorsViewModel* view_model = model;
    many_sensors_draw(canvas, view_model);
}

static bool many_sensors_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    ManySensors* many_sensors = context;
    UnitempApp* app = many_sensors->context;
    bool consumed = false;

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        scene_manager_next_scene(app->scene_manager, UnitempSceneMenu);
        consumed = true;
    } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
        if(++app->settings->temperature_unit >= UT_TEMP_COUNT) app->settings->temperature_unit = 0;
        consumed = true;
    } else if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        uint8_t pages =
            unitemp_sensors_get_count() / 4 + (unitemp_sensors_get_count() % 4 ? 1 : 0);
        with_view_model(
            many_sensors->view,
            ManySensorsViewModel * model,
            {
                if(--model->sensors_page >= pages) {
                    model->sensors_page = pages - 1;
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
        uint8_t pages =
            unitemp_sensors_get_count() / 4 + (unitemp_sensors_get_count() % 4 ? 1 : 0);
        with_view_model(
            many_sensors->view,
            ManySensorsViewModel * model,
            {
                if(++model->sensors_page >= pages) {
                    model->sensors_page = 0;
                }
            },
            true);
        consumed = true;
    } else if(event->key == InputKeyDown && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, CustomEventSwitchToSingleSensorView);
        consumed = true;
    }

    return consumed;
}

ManySensors* many_sensors_alloc(void* context) {
    UnitempApp* app = context;
    ManySensors* many_sensors = malloc(sizeof(ManySensors));

    many_sensors->view = view_alloc();
    many_sensors->context = app;
    view_allocate_model(many_sensors->view, ViewModelTypeLockFree, sizeof(ManySensorsViewModel));

    with_view_model(
        many_sensors->view,
        ManySensorsViewModel * model,
        {
            model->sensors_page = 0;
            model->context = app;
        },
        false);

    view_set_context(many_sensors->view, many_sensors);
    view_set_draw_callback(many_sensors->view, many_sensors_draw_callback);
    view_set_input_callback(many_sensors->view, many_sensors_input_callback);

    return many_sensors;
}

void many_sensors_free(ManySensors* many_sensors) {
    furi_assert(many_sensors);
    view_free(many_sensors->view);
    free(many_sensors);
}

View* many_sensors_get_view(ManySensors* many_sensors) {
    furi_assert(many_sensors);
    return many_sensors->view;
}

void many_sensors_refresh_data(ManySensors* instance) {
    furi_assert(instance);

    //Вызываем перерисовку вида псевдообновлением модели. Вызывается по таймеру каждую секнуду
    with_view_model(instance->view, ManySensorsViewModel * model, { UNUSED(model); }, true);
}
