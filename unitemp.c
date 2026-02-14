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

#include "unitemp.h"

#include <stdlib.h>

#include <core/thread.h>
#include <core/kernel.h>
#include <locale/locale.h>
#include "flipper_format.h"

bool unitemp_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    UnitempApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool unitemp_back_event_callback(void* context) {
    furi_assert(context);
    UnitempApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void unitemp_tick_event_callback(void* context) {
    furi_assert(context);
    UnitempApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void unitemp_submenu_callback(void* context, uint32_t index) {
    UnitempApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

bool unitemp_settings_load(void* context) {
    if(context == NULL) return false;

    UnitempApp* app = context;
    FURI_LOG_I(APP_NAME, "Loading settings...");

    //Установка значений по умолчанию
    app->settings->infinity_backlight = true;
    LocaleMeasurementUnits lmu = locale_get_measurement_unit();
    //Установка единиц измерения температуры в соответствии с системными настройками
    if(lmu == LocaleMeasurementUnitsImperial) {
        app->settings->temperature_unit = UT_TEMP_FAHRENHEIT;
        app->settings->pressure_unit = UT_PRESSURE_IN_HG;
    } else {
        app->settings->temperature_unit = UT_TEMP_CELSIUS;
        app->settings->pressure_unit = UT_PRESSURE_MM_HG;
    }
    app->settings->humidity_unit = UT_HUMIDITY_RELATIVE;
    app->settings->heat_index = false;
    app->settings->last_otg_state = power_is_otg_enabled(app->power);

    bool result = false;
    FlipperFormat* file = flipper_format_file_alloc(app->storage);

    uint32_t uint32_value = 1;
    FuriString* file_type;
    file_type = furi_string_alloc();
    do {
        if(!flipper_format_file_open_existing(file, APP_DATA_PATH(APP_SETTINGS_FILENAME))) break;

        // Reading settings from a file. If any key is not read, the default value will be used
        flipper_format_read_uint32(file, "infinity_backlight", &uint32_value, 1);
        app->settings->infinity_backlight = uint32_value;
        flipper_format_read_uint32(file, "temperature_unit", &uint32_value, 1);
        app->settings->temperature_unit = uint32_value;
        flipper_format_read_uint32(file, "humidity_unit", &uint32_value, 1);
        app->settings->humidity_unit = uint32_value;
        flipper_format_read_uint32(file, "pressure_unit", &uint32_value, 1);
        app->settings->pressure_unit = uint32_value;
        flipper_format_read_uint32(file, "heat_index", &uint32_value, 1);
        app->settings->heat_index = uint32_value;

        result = true;
    } while(0);

    furi_string_free(file_type);
    flipper_format_free(file);
    UNITEMP_DEBUG("Loading settings  %s", result ? "success" : "failed");

    return result;
}

bool unitemp_settings_save(void* context) {
    if(context == NULL) return false;
    UnitempApp* app = context;
    FURI_LOG_I(APP_NAME, "Saving settings...");

    FlipperFormat* file = flipper_format_file_alloc(app->storage);

    bool result = false;

    do {
        if(!flipper_format_file_open_always(file, APP_DATA_PATH(APP_SETTINGS_FILENAME))) break;

        if(!flipper_format_write_comment_cstr(file, "Unitemp config file. Don't modify manually"))
            break;
        uint32_t buff = app->settings->infinity_backlight;
        if(!flipper_format_write_uint32(file, "infinity_backlight", &buff, 1)) break;
        buff = app->settings->temperature_unit;
        if(!flipper_format_write_uint32(file, "temperature_unit", &buff, 1)) break;
        buff = app->settings->humidity_unit;
        if(!flipper_format_write_uint32(file, "humidity_unit", &buff, 1)) break;
        buff = app->settings->pressure_unit;
        if(!flipper_format_write_uint32(file, "pressure_unit", &buff, 1)) break;
        buff = app->settings->heat_index;
        if(!flipper_format_write_uint32(file, "heat_index", &buff, 1)) break;

        result = true;
    } while(0);

    flipper_format_free(file);
    if(!result) {
        FURI_LOG_E(APP_NAME, "Failed to save settings");
    }

    UNITEMP_DEBUG("Saving settings  %s", result ? "success" : "failed");
    return result;
}

static UnitempApp* unitemp_app_alloc(void) {
    UnitempApp* app = malloc(sizeof(UnitempApp));

    app->settings = malloc(sizeof(UnitempSettings));
    app->storage = furi_record_open(RECORD_STORAGE);
    app->file = storage_file_alloc(app->storage);

    app->reader_thread = furi_thread_alloc();
    furi_thread_set_stack_size(app->reader_thread, 1024U);
    furi_thread_set_context(app->reader_thread, app);
    furi_thread_set_callback(app->reader_thread, unitemp_sensors_update_callback);

    app->power = furi_record_open(RECORD_POWER);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    //GUI allocations
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&unitemp_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(app->view_dispatcher, unitemp_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, unitemp_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, unitemp_tick_event_callback, 250);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UnitempViewSubmenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, UnitempViewPopup, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UnitempViewWidget, widget_get_view(app->widget));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        UnitempViewVariableList,
        variable_item_list_get_view(app->var_item_list));

    app->no_sensors = no_sensors_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, UnitempViewNoSensors, no_sensors_get_view(app->no_sensors));
    app->single_sensor = single_sensor_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, UnitempViewSingleSensor, single_sensor_get_view(app->single_sensor));
    app->temp_overview = temp_overview_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, UnitempViewTempOverview, temp_overview_get_view(app->temp_overview));
    return app;
}

static void unitemp_app_free(UnitempApp* app) {
    furi_check(app);

    unitemp_sensors_free();

    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewTempOverview);
    temp_overview_free(app->temp_overview);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewSingleSensor);
    single_sensor_free(app->single_sensor);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewNoSensors);
    no_sensors_free(app->no_sensors);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewVariableList);
    variable_item_list_free(app->var_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewWidget);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewPopup);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_thread_free(app->reader_thread);

    storage_file_free(app->file);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_POWER);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app->settings);
    free(app);
}

/* Starts the reader thread and handles the input */
static void unitemp_run(UnitempApp* app) {
    furi_check(app);
    if(!unitemp_settings_load(app)) {
        FURI_LOG_W(
            APP_NAME, "Settings file not found or corrupted. Using defaults and saving them");
        unitemp_settings_save(app);
    }
    unitemp_sensors_load();
    unitemp_sensors_init(app);
    /* Start the reader thread. It will talk to the thermometer in the background. */
    furi_thread_start(app->reader_thread);

    scene_manager_next_scene(app->scene_manager, UnitempSceneMonitor);
    view_dispatcher_run(app->view_dispatcher);
}

static void unitemp_stop(UnitempApp* app) {
    furi_check(app);
    /* Signal the reader thread to cease operation and exit */
    furi_thread_flags_set(furi_thread_get_id(app->reader_thread), UnitempThreadFlagExit);

    /* Wait for the reader thread to finish */
    furi_thread_join(app->reader_thread);

    unitemp_sensors_deinit(app);
}
/**
 * @brief Точка входа в приложение
 * 
 * @return Код ошибки
 */
int32_t unitemp_app() {
    UNITEMP_DEBUG("Unitemp application started");
    UnitempApp* app = unitemp_app_alloc();
    unitemp_run(app);

    unitemp_stop(app);
    unitemp_app_free(app);

    UNITEMP_DEBUG("Unitemp application finished");
    return 0;
}
