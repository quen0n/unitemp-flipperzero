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
#include "view_temp_overview.h"
#include "../unitemp.h"
#include "../helpers/unitemp_draw.h"
#include "../helpers/unitemp_utils.h"

#include <stdlib.h>
#include <gui/elements.h>
#include <locale/locale.h>

extern const Icon I_ButtonRight_4x7;
extern const Icon I_ButtonLeft_4x7;

/* Picks the CO2 (or combo) sensor shown on the given overview page, or NULL. */
static Sensor* overview_page_co2_sensor(uint8_t sensors_page) {
    uint8_t count = unitemp_sensors_get_count();
    uint8_t pages = count / 4 + (count % 4 ? 1 : 0);
    if(pages == 0) return NULL;
    if(sensors_page >= pages) sensors_page = 0;
    uint8_t base = sensors_page * 4;
    uint8_t on_page = ((count - base) / 4) ? 4 : ((count - base) % 4);
    for(uint8_t i = 0; i < on_page; i++) {
        Sensor* s = unitemp_sensors_get(base + i);
        SensorDataType dt = s->model->data_type;
        if(dt == UT_DATA_TYPE_CO2 || dt == UT_DATA_TYPE_TEMP_HUM_CO2) return s;
    }
    return NULL;
}

struct TempOverview {
    View* view;
    void* context;
};

typedef struct {
    uint8_t sensors_page;
    void* context;
} TempOverviewViewModel;

void temp_overview_draw(Canvas* canvas, TempOverviewViewModel* view_model) {
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
            unitemp_sensors_get(view_model->sensors_page * 4 + i),
            settings->temperature_unit,
            value_positions[page_sensors_count - 1][i][0],
            value_positions[page_sensors_count - 1][i][1]);
    }
}

static void temp_overview_draw_callback(Canvas* canvas, void* model) {
    TempOverviewViewModel* view_model = model;

    uint8_t pages = unitemp_sensors_get_count() / 4 + (unitemp_sensors_get_count() % 4 ? 1 : 0);
    if(view_model->sensors_page > pages) {
        view_model->sensors_page = pages - 1;
    }

    temp_overview_draw(canvas, view_model);
}

static bool temp_overview_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    TempOverview* temp_overview = context;
    UnitempApp* app = temp_overview->context;
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
            temp_overview->view,
            TempOverviewViewModel * model,
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
            temp_overview->view,
            TempOverviewViewModel * model,
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

TempOverview* temp_overview_alloc(void* context) {
    UnitempApp* app = context;
    TempOverview* temp_overview = malloc(sizeof(TempOverview));

    temp_overview->view = view_alloc();
    temp_overview->context = app;
    view_allocate_model(temp_overview->view, ViewModelTypeLockFree, sizeof(TempOverviewViewModel));

    with_view_model(
        temp_overview->view,
        TempOverviewViewModel * model,
        {
            model->sensors_page = 0;
            model->context = app;
        },
        false);

    view_set_context(temp_overview->view, temp_overview);
    view_set_draw_callback(temp_overview->view, temp_overview_draw_callback);
    view_set_input_callback(temp_overview->view, temp_overview_input_callback);

    return temp_overview;
}

void temp_overview_free(TempOverview* temp_overview) {
    furi_assert(temp_overview);
    view_free_model(temp_overview->view);
    view_free(temp_overview->view);
    free(temp_overview);
}

View* temp_overview_get_view(TempOverview* temp_overview) {
    furi_assert(temp_overview);
    return temp_overview->view;
}

void temp_overview_refresh_data(TempOverview* instance) {
    furi_assert(instance);

    //Lamp follows THIS page's tiles, driven from the scene tick like the carousel
    //(not from the draw callback): a CO2 tile present -> CO2 owns the lamp;
    //otherwise no CO2 on screen -> lamp off. overview_page_co2_sensor self-guards
    //the page bounds. The `true` also marks the model dirty to trigger a redraw.
    with_view_model(
        instance->view,
        TempOverviewViewModel * model,
        {
            unitemp_indication_tick(
                model->context, overview_page_co2_sensor(model->sensors_page), NULL);
        },
        true);
}
