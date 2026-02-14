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

#include "DS18x2x.h"
#include "interfaces/onewire_sensor.h"

const SensorModel Dallas = {
    .modelname = "Dallas",
    .altname = "Dallas (DS18x2x)",
    .interface = &onewire,
    .data_type = UT_DATA_TYPE_TEMP,
    .polling_interval = 750,
    .allocator = unitemp_onewire_sensor_alloc,
    .mem_releaser = unitemp_onewire_sensor_free,
    .initializer = unitemp_onewire_sensor_init,
    .deinitializer = unitemp_onewire_sensor_deinit,
    .updater = unitemp_onewire_sensor_update};
