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

#include "../unitemp.h"

typedef enum {
    NoSensorsViewMode,
    SingleSensorViewMode,
    ListSensorViewMode,

    ViewModesCount
} GeneralViewMode;

typedef struct {
    GeneralViewMode view_mode;
    void* context;
} UnitempSceneGeneralStruct;

UnitempSceneGeneralStruct* unitemp_scene_general_data;

void unitemp_scene_general_on_enter(void* context) {
    furi_assert(context);
    UnitempApp* app = context;
    unitemp_scene_general_data = malloc(sizeof(UnitempSceneGeneralStruct));

    if(app->settings->infinity_backlight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    if(unitemp_sensors_get_count() == 0) {
        unitemp_scene_general_data->view_mode = NoSensorsViewMode;
        view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewNoSensors);
    } else {
        unitemp_scene_general_data->view_mode = SingleSensorViewMode;
        view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewSingleSensor);
    }
}

bool unitemp_scene_general_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(unitemp_scene_general_data->view_mode == SingleSensorViewMode) {
            single_sensor_refresh_data(app->single_sensor);
        }
        consumed = true;
    }

    return consumed;
}

void unitemp_scene_general_on_exit(void* context) {
    furi_assert(context);
    UnitempApp* app = context;
    if(app->settings->infinity_backlight) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }
    free(unitemp_scene_general_data);
}
