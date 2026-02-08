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
    UnitempApp* app = context;
    FURI_LOG_I(APP_NAME, "Loading settings...");

    //Установка значений по умолчанию
    app->settings->infinity_backlight = true;
    LocaleMeasurementUnits lmu = locale_get_measurement_unit();
    //Установка единиц измерения температуры в соответствии с системными настройками
    if(lmu == LocaleMeasurementUnitsImperial) {
        app->settings->temp_unit = UT_TEMP_FAHRENHEIT;
        app->settings->pressure_unit = UT_PRESSURE_IN_HG;
    } else {
        app->settings->temp_unit = UT_TEMP_CELSIUS;
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
        flipper_format_read_uint32(file, "temp_unit", &uint32_value, 1);
        app->settings->temp_unit = uint32_value;
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
        buff = app->settings->temp_unit;
        if(!flipper_format_write_uint32(file, "temp_unit", &buff, 1)) break;
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

    return app;
}

static void unitemp_app_free(UnitempApp* app) {
    unitemp_sensors_free();

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
    if(!unitemp_settings_load(app)) {
        FURI_LOG_W(
            APP_NAME, "Settings file not found or corrupted. Using defaults and saving them");
        unitemp_settings_save(app);
    }
    unitemp_sensors_load();
    unitemp_sensors_init(app);
    /* Start the reader thread. It will talk to the thermometer in the background. */
    furi_thread_start(app->reader_thread);

    scene_manager_next_scene(app->scene_manager, UnitempSceneGeneral);
    view_dispatcher_run(app->view_dispatcher);
}
static void unitemp_stop(UnitempApp* app) {
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

/*bool open_result = storage_file_open(
        app->file, APP_DATA_PATH(APP_SETTINGS_FILENAME), FSAM_READ, FSOM_OPEN_EXISTING);

    if(!open_result) {
        storage_file_close(app->file);
        //Если файла нет, попытка миграции с версии 1.х
        FURI_LOG_W(APP_NAME, "Settings file not found, trying to migrate from v1.x...");
        open_result = storage_file_open(
            app->file, "/ext/unitemp/settings.cfg", FSAM_READ, FSOM_OPEN_EXISTING);
        if(!open_result) {
            storage_file_close(app->file);
            FURI_LOG_W(APP_NAME, "No old settings file found, using defaults");

            return true;
        }
    }

    uint32_t file_size = storage_file_size(app->file);
    uint8_t* file_buf = malloc(file_size);
    // Clear the file buffer
    memset(file_buf, 0, file_size);
    // Load the file
    if(storage_file_read(app->file, file_buf, file_size) != file_size) {
        // Exit on read error
        FURI_LOG_E(APP_NAME, "Error reading settings file");
        storage_file_close(app->file);
        //Free memory
        free(file_buf);
        return false;
    }
    // Read the file line by line
    // Pointer to the start of the line
    FuriString* file = furi_string_alloc_set_str((char*)file_buf);
    // How many bytes to the end of the line
    size_t line_end = 0;

    while(line_end != ((size_t)-1) && line_end != (size_t)(file_size - 1)) {
        char buff[20] = {0};
        sscanf(((char*)(file_buf + line_end)), "%s", buff);

        if(!strcmp(buff, "INFINITY_BACKLIGHT")) {
            // Read the parameter value
            int p = 0;
            sscanf(((char*)(file_buf + line_end)), "INFINITY_BACKLIGHT %d", &p);
            app->settings->infinity_backlight = p;
            UNITEMP_DEBUG("INFINITY_BACKLIGHT %d", app->settings->infinity_backlight);
        } else if(!strcmp(buff, "TEMP_UNIT")) {
            // Read the parameter value
            int p = 0;
            sscanf(((char*)(file_buf + line_end)), "\nTEMP_UNIT %d", &p);
            app->settings->temp_unit = p;
            UNITEMP_DEBUG("TEMP_UNIT %d", app->settings->temp_unit);
        } else if(!strcmp(buff, "HUMIDITY_UNIT")) {
            int p = 9;
            sscanf(((char*)(file_buf + line_end)), "\nHUMIDITY_UNIT %d", &p);
            app->settings->humidity_unit = p;
            UNITEMP_DEBUG("HUMIDITY_UNIT %d", app->settings->humidity_unit);
        } else if(!strcmp(buff, "PRESSURE_UNIT")) {
            // Read the parameter value
            int p = 0;
            sscanf(((char*)(file_buf + line_end)), "\nPRESSURE_UNIT %d", &p);
            app->settings->pressure_unit = p;
            UNITEMP_DEBUG("PRESSURE_UNIT %d", app->settings->pressure_unit);
        } else if(!strcmp(buff, "HEAT_INDEX")) {
            // Read the parameter value
            int p = 0;
            sscanf(((char*)(file_buf + line_end)), "\nHEAT_INDEX %d", &p);
            app->settings->heat_index = p;
            UNITEMP_DEBUG("HEAT_INDEX %d", app->settings->heat_index);
        } else {
            FURI_LOG_W(APP_NAME, "Unknown settings parameter: %s", buff);
        }

        // Calculate the end of the line
        line_end = furi_string_search_char(file, '\n', line_end + 1);
    }
    free(file_buf);
    furi_string_free(file);

    storage_file_close(app->file);*/
