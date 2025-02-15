/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2023  Victor Nikitchuk (https://github.com/quen0n)

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
#include "SHTC3.h"
#include "../interfaces/I2CSensor.h"

const SensorType SHTC3 = {
    .typename = "SHTC3",
    .altname = "SHTC3",
    .interface = &I2C,
    .datatype = UT_TEMPERATURE | UT_HUMIDITY,
    .pollingInterval = 1000,
    .allocator = unitemp_SHTC3_I2C_alloc,
    .mem_releaser = unitemp_SHTC3_I2C_free,
    .initializer = unitemp_SHTC3_init,
    .deinitializer = unitemp_SHTC3_I2C_deinit,
    .updater = unitemp_SHTC3_I2C_update};

bool unitemp_SHTC3_I2C_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Адреса на шине I2C (7 бит)
    i2c_sensor->minI2CAdr = 0x70 << 1;
    i2c_sensor->maxI2CAdr = 0x70 << 1;
    return true;
}

bool unitemp_SHTC3_I2C_free(Sensor* sensor) {
    //Нечего высвобождать, так как ничего не было выделено
    UNUSED(sensor);
    return true;
}

bool unitemp_SHTC3_init(Sensor* sensor) {
    UNUSED(sensor);
    return true;
}

bool unitemp_SHTC3_I2C_deinit(Sensor* sensor) {
    //Нечего деинициализировать
    UNUSED(sensor);
    return true;
}

UnitempStatus unitemp_SHTC3_I2C_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    //Получение данных
    uint8_t data[6] = {0x7C, 0xA2};
    if(!unitemp_i2c_writeArray(i2c_sensor, 2, data)) return UT_SENSORSTATUS_TIMEOUT;
    if(!unitemp_i2c_readArray(i2c_sensor, 6, data)) return UT_SENSORSTATUS_TIMEOUT;

    uint32_t temp = 175 * ((uint16_t)(data[0] << 8) | data[1]);
    uint32_t hum  = 100 * ((uint16_t)(data[3] << 8) | data[4]);

    sensor->temp = -45 + temp / 65536.0f;
    sensor->hum = hum / 65536.0f;

    return UT_SENSORSTATUS_OK;
}
