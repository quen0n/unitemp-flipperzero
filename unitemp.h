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
#ifndef UNITEMP_H_
#define UNITEMP_H_

#include <gui/gui.h>
#include <gui/view_port.h>

#include <power/power_service/power.h>

#include "sensors.h"

/* Declaring Macro Substitutions */
//Application name
#define APP_NAME        "Unitemp"
//Application version
#define UNITEMP_APP_VER "2.0.0-dev"

//Text buffer size
#define TEXT_STORE_SIZE 32

//Debug macro (only if app builded in debug mode)
#ifdef FURI_DEBUG
#define UNITEMP_DEBUG(msg, ...) FURI_LOG_D(APP_NAME, msg, ##__VA_ARGS__)
#else
#define UNITEMP_DEBUG(msg, ...)
#endif

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriThread* reader_thread;
    FuriMessageQueue* event_queue;
    Power* power;
} UnitempApp;

/**
 * @brief Get pointer to the application context
 * @return Pointer to UnitempApp structure
 */
UnitempApp* unitemp_get_app(void);

#endif //UNITEMP_H_
