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
#include "sensors.h"
#include "scenes/unitemp_scene.h"

void unitemp_scene_sensors_list_on_enter(void* context) {
    UnitempApp* app = context;
    Submenu* submenu = app->submenu;

    uint8_t sensors_type_count = unitemp_sensors_models_get_count();
    const SensorModel** models = unitemp_sensors_models_get();

    for(uint8_t i = 0; i < (sensors_type_count); i++) {
        submenu_add_item(
            submenu,
            (models[i]->altname == NULL) ? models[i]->modelname : models[i]->altname,
            i,
            unitemp_submenu_callback,
            app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, UnitempSceneSensorsList));

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewSubmenu);
}

bool unitemp_scene_sensors_list_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, UnitempSceneSensorsList, event.event);
        consumed = true;
        // if(event.event == SubmenuIndexAddNewSensor) {
        //     // scene_manager_next_scene(app->scene_manager, UnitempSceneAddNewSensor);
        // } else if(event.event == SubmenuIndexSettings) {
        //     scene_manager_next_scene(app->scene_manager, UnitempSceneSettings);
        // } else if(event.event == SubmenuIndexHelp) {
        //     scene_manager_next_scene(app->scene_manager, UnitempSceneHelp);
        // } else if(event.event == SubmenuIndexAbout) {
        //     scene_manager_next_scene(app->scene_manager, UnitempSceneAbout);
        // }
    }

    return consumed;
}

void unitemp_scene_sensors_list_on_exit(void* context) {
    UnitempApp* app = context;
    submenu_reset(app->submenu);
}
