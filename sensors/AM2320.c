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

#include "AM2320.h"
#include "interfaces/singlewire.h"

const SensorModel AM2320_SW = {
    .modelname = "AM2320",
    .altname = "AM2320 (single wire)",
    .interface = &SINGLE_WIRE,
    .data_type = UT_DATA_TYPE_TEMP_HUM,
    .polling_interval = 2000,
    .allocator = unitemp_singlewire_alloc,
    .mem_releaser = unitemp_singlewire_free,
    .initializer = unitemp_singlewire_init,
    .deinitializer = unitemp_singlewire_deinit,
    .updater = unitemp_singlewire_update};
