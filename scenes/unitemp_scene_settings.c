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

static const char unitemp_scene_settings_backlight_text[2][9] = {"System", "Infinity"};
static const char unitemp_scene_settings_temp_units_text[UT_TEMP_COUNT][3] = {"*C", "*F"};
static const char unitemp_scene_settings_humidity_units_text[UT_HUMIDITY_COUNT][12] = {
    "Relative",
    "Dewpoint"};
static const char unitemp_scene_settings_pressure_units_text[UT_PRESSURE_COUNT][6] =
    {"mmHg", "inHg", "kPa", "hPa"};
static const char unitemp_scene_settings_heat_index_text[2][4] = {"OFF", "ON"};

static void unitemp_scene_settings_backlight_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, unitemp_scene_settings_backlight_text[index]);
    app->settings->infinity_backlight = (bool)index;
    UNITEMP_DEBUG("Infinity backlight set to %s", unitemp_scene_settings_backlight_text[index]);
}

static void unitemp_scene_settings_temp_units_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, unitemp_scene_settings_temp_units_text[index]);
    app->settings->temp_unit = index;
    UNITEMP_DEBUG("Temperature unit set to %s", unitemp_scene_settings_temp_units_text[index]);
}

static void unitemp_scene_settings_humidity_units_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, unitemp_scene_settings_humidity_units_text[index]);
    app->settings->humidity_unit = index;
    UNITEMP_DEBUG("Humidity unit set to %s", unitemp_scene_settings_humidity_units_text[index]);
}

static void unitemp_scene_settings_pressure_units_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, unitemp_scene_settings_pressure_units_text[index]);
    app->settings->pressure_unit = index;
    UNITEMP_DEBUG("Pressure unit set to %s", unitemp_scene_settings_pressure_units_text[index]);
}

static void unitemp_scene_settings_heat_index_change_callback(VariableItem* item) {
    UnitempApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, unitemp_scene_settings_heat_index_text[index]);
    app->settings->heat_index = (bool)index;
    UNITEMP_DEBUG(
        "Heat index calculation set to %s", unitemp_scene_settings_heat_index_text[index]);
}
void unitemp_scene_settings_on_enter(void* context) {
    UnitempApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;
    uint8_t value_index;

    //Resetting the last selected item
    variable_item_list_set_selected_item(var_item_list, 0);

    item = variable_item_list_add(
        var_item_list,
        "Backlight time",
        COUNT_OF(unitemp_scene_settings_backlight_text),
        unitemp_scene_settings_backlight_change_callback,
        app);
    value_index = app->settings->infinity_backlight;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, unitemp_scene_settings_backlight_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Temp. unit",
        COUNT_OF(unitemp_scene_settings_temp_units_text),
        unitemp_scene_settings_temp_units_change_callback,
        app);
    value_index = app->settings->temp_unit;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(
        item, unitemp_scene_settings_temp_units_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Humidity unit",
        COUNT_OF(unitemp_scene_settings_humidity_units_text),
        unitemp_scene_settings_humidity_units_change_callback,
        app);
    value_index = app->settings->humidity_unit;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(
        item, unitemp_scene_settings_humidity_units_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Pressure unit",
        COUNT_OF(unitemp_scene_settings_pressure_units_text),
        unitemp_scene_settings_pressure_units_change_callback,
        app);
    value_index = app->settings->pressure_unit;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(
        item, unitemp_scene_settings_pressure_units_text[value_index]);

    item = variable_item_list_add(
        var_item_list,
        "Calc. heat index",
        COUNT_OF(unitemp_scene_settings_heat_index_text),
        unitemp_scene_settings_heat_index_change_callback,
        app);
    value_index = app->settings->heat_index;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(
        item, unitemp_scene_settings_heat_index_text[value_index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewVariableList);
}

bool unitemp_scene_settings_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;

    UnitempApp* app = context;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void unitemp_scene_settings_on_exit(void* context) {
    UnitempApp* app = context;
    variable_item_list_reset(app->var_item_list);
    unitemp_settings_save(app);
}
