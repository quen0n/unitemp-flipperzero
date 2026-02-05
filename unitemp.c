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

#include <core/thread.h>
#include <core/kernel.h>
#include <stdlib.h>

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

static UnitempApp* unitemp_app_alloc(void) {
    UnitempApp* app = malloc(sizeof(UnitempApp));

    app->scene_manager = scene_manager_alloc(&unitemp_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, unitemp_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, unitemp_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, unitemp_tick_event_callback, 100);

    app->reader_thread = furi_thread_alloc();
    furi_thread_set_stack_size(app->reader_thread, 1024U);
    furi_thread_set_context(app->reader_thread, app);
    furi_thread_set_callback(app->reader_thread, unitemp_sensors_update_callback);

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->power = furi_record_open(RECORD_POWER);
    app->gui = furi_record_open(RECORD_GUI);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UnitempViewSubmenu, submenu_get_view(app->submenu));
    app->popup = popup_alloc();

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, UnitempViewWidget, widget_get_view(app->widget));

    view_dispatcher_add_view(app->view_dispatcher, UnitempViewPopup, popup_get_view(app->popup));
    return app;
}

static void unitemp_app_free(UnitempApp* app) {
    unitemp_sensors_free();

    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewWidget);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewPopup);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, UnitempViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_thread_free(app->reader_thread);
    furi_message_queue_free(app->event_queue);

    furi_record_close(RECORD_POWER);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* Starts the reader thread and handles the input */
static void unitemp_run(UnitempApp* app) {
    unitemp_sensors_load();
    unitemp_sensors_init(app);
    /* Start the reader thread. It will talk to the thermometer in the background. */
    furi_thread_start(app->reader_thread);
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

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, UnitempSceneMenu);

    view_dispatcher_run(app->view_dispatcher);

    unitemp_stop(app);
    unitemp_app_free(app);

    UNITEMP_DEBUG("Unitemp application finished");
    return 0;
}

// static void unitemp_draw_callback(Canvas* canvas, void* ctx) {
//     UnitempApp* app = ctx;
//     UNUSED(app);
//     char text_store[TEXT_STORE_SIZE];
//     const size_t middle_x = canvas_width(canvas) / 2U;

//     canvas_set_font(canvas, FontPrimary);
//     canvas_draw_str_aligned(canvas, middle_x, 12, AlignCenter, AlignBottom, "Thermometer Demo");
//     canvas_draw_line(canvas, 0, 16, 128, 16);

//     canvas_set_font(canvas, FontSecondary);
//     canvas_draw_str_aligned(canvas, middle_x, 30, AlignCenter, AlignBottom, "Connect thermometer");

//     if(unitemp_sensors_get_count() == 0) {
//         canvas_draw_str_aligned(
//             canvas, middle_x, 42, AlignCenter, AlignBottom, "No sensors loaded");
//         return;
//     } else {
//         snprintf(
//             text_store,
//             TEXT_STORE_SIZE,
//             "to GPIO pin %s",
//             ((SingleWireSensor*)(unitemp_sensors_get()[0]->instance))->gpio_pin->name);
//         canvas_draw_str_aligned(canvas, middle_x, 42, AlignCenter, AlignBottom, text_store);

//         canvas_set_font(canvas, FontKeyboard);

//         /* If a reading is available, display it */
//         snprintf(
//             text_store,
//             TEXT_STORE_SIZE,
//             "Temperature: %+.1f°C",
//             (double)unitemp_sensors_get()[0]->temp);

//         canvas_draw_str_aligned(canvas, middle_x, 58, AlignCenter, AlignBottom, text_store);
//     }
// }

// /* This function is called from the GUI thread. All it does is put the event
//    into the application's queue so it can be processed later. */
// static void unitemp_input_callback(InputEvent* event, void* ctx) {
//     UnitempApp* app = ctx;
//     furi_message_queue_put(app->event_queue, event, FuriWaitForever);
// }
