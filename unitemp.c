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
#include "interfaces/singlewire.h"

#include <core/thread.h>
#include <core/kernel.h>

static void unitemp_draw_callback(Canvas* canvas, void* ctx) {
    UnitempApp* app = ctx;
    UNUSED(app);
    char text_store[TEXT_STORE_SIZE];
    const size_t middle_x = canvas_width(canvas) / 2U;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, middle_x, 12, AlignCenter, AlignBottom, "Thermometer Demo");
    canvas_draw_line(canvas, 0, 16, 128, 16);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, middle_x, 30, AlignCenter, AlignBottom, "Connect thermometer");

    if(unitemp_sensors_get_count() == 0) {
        canvas_draw_str_aligned(
            canvas, middle_x, 42, AlignCenter, AlignBottom, "No sensors loaded");
        return;
    } else {
        snprintf(
            text_store,
            TEXT_STORE_SIZE,
            "to GPIO pin %s",
            ((SingleWireSensor*)(unitemp_sensors_get()[0]->instance))->gpio_pin->name);
        canvas_draw_str_aligned(canvas, middle_x, 42, AlignCenter, AlignBottom, text_store);

        canvas_set_font(canvas, FontKeyboard);

        /* If a reading is available, display it */
        snprintf(
            text_store,
            TEXT_STORE_SIZE,
            "Temperature: %+.1f°C",
            (double)unitemp_sensors_get()[0]->temp);

        canvas_draw_str_aligned(canvas, middle_x, 58, AlignCenter, AlignBottom, text_store);
    }
}

/* This function is called from the GUI thread. All it does is put the event
   into the application's queue so it can be processed later. */
static void unitemp_input_callback(InputEvent* event, void* ctx) {
    UnitempApp* app = ctx;
    furi_message_queue_put(app->event_queue, event, FuriWaitForever);
}

static UnitempApp* unitemp_app_alloc(void) {
    UnitempApp* app = malloc(sizeof(UnitempApp));

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, unitemp_draw_callback, app);
    view_port_input_callback_set(app->view_port, unitemp_input_callback, app);

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->reader_thread = furi_thread_alloc();
    furi_thread_set_stack_size(app->reader_thread, 1024U);
    furi_thread_set_context(app->reader_thread, app);
    furi_thread_set_callback(app->reader_thread, unitemp_sensors_update_callback);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->power = furi_record_open(RECORD_POWER);

    return app;
}

static void unitemp_app_free(UnitempApp* app) {
    unitemp_sensors_free();

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);

    furi_thread_free(app->reader_thread);
    furi_message_queue_free(app->event_queue);
    view_port_free(app->view_port);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_POWER);

    free(app);
}

/* Starts the reader thread and handles the input */
static void unitemp_run(UnitempApp* app) {
    unitemp_sensors_load();
    unitemp_sensors_init(app);

    /* Start the reader thread. It will talk to the thermometer in the background. */
    furi_thread_start(app->reader_thread);

    /* An endless loop which handles the input*/
    for(bool is_running = true; is_running;) {
        InputEvent event;
        /* Wait for an input event. Input events come from the GUI thread via a callback. */
        const FuriStatus status =
            furi_message_queue_get(app->event_queue, &event, FuriWaitForever);

        /* This application is only interested in short button presses. */
        if((status != FuriStatusOk) || (event.type != InputTypeShort)) {
            continue;
        }

        /* When the user presses the "Back" button, break the loop and exit the application. */
        if(event.key == InputKeyBack) {
            is_running = false;
        }
    }

    /* Signal the reader thread to cease operation and exit */
    furi_thread_flags_set(furi_thread_get_id(app->reader_thread), UnitempThreadFlagExit);

    /* Wait for the reader thread to finish */
    furi_thread_join(app->reader_thread);
}
static void unitemp_stop(UnitempApp* app) {
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
