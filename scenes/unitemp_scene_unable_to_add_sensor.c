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

#include "unitemp_icons.h"

static void unitemp_scene_unable_to_add_sensor_callback(void* context) {
    UnitempApp* app = context;
    UNUSED(app);
    view_dispatcher_send_custom_event(app->view_dispatcher, CustonEventBack);
}

void unitemp_scene_unable_to_add_sensor_on_enter(void* context) {
    UnitempApp* app = context;
    Popup* popup = app->popup;

    popup_set_icon(app->popup, 0, 64 - icon_get_height(&I_Cry_dolph_55x52), &I_Cry_dolph_55x52);
    popup_set_header(app->popup, "No GPIO's available", 64, 6, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        "Free ports 15,16\nfor I2C or 1,2,4\n and any for SPI",
        (128 - icon_get_width(&I_Cry_dolph_55x52)) / 2 + icon_get_width(&I_Cry_dolph_55x52),
        32,
        AlignCenter,
        AlignCenter);

    popup_set_callback(popup, unitemp_scene_unable_to_add_sensor_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 5000);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewPopup);
}

bool unitemp_scene_unable_to_add_sensor_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == CustonEventBack) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, UnitempSceneSensorsList);
        }
    }

    return consumed;
}

void unitemp_scene_unable_to_add_sensor_on_exit(void* context) {
    UnitempApp* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
}
