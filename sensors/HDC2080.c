/*
    Unitemp - Universal temperature reader
    Copyright (C) 2023  Victor Nikitchuk (https://github.com/quen0n)

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
#include "HDC2080.h"
#include "../interfaces/I2CSensor.h"

const SensorType HDC2080 = {
    .typename = "HDC2080",
    .interface = &I2C,
    .datatype = UT_DATA_TYPE_TEMP_HUM,
    .pollingInterval = 250,
    .allocator = unitemp_HDC2080_alloc,
    .mem_releaser = unitemp_HDC2080_free,
    .initializer = unitemp_HDC2080_init,
    .deinitializer = unitemp_HDC2080_deinit,
    .updater = unitemp_HDC2080_update};

bool unitemp_HDC2080_alloc(Sensor* sensor, char* args) {
    UNUSED(args);
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    //Адреса на шине I2C (7 бит)
    i2c_sensor->minI2CAdr = 0x40 << 1;
    i2c_sensor->maxI2CAdr = 0x40 << 1;
    return true;
}

bool unitemp_HDC2080_free(Sensor* sensor) {
    //Нечего высвобождать, так как ничего не было выделено
    UNUSED(sensor);
    return true;
}

bool unitemp_HDC2080_init(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t data[2];
    if(!unitemp_i2c_readRegArray(i2c_sensor, 0xFF, 2, data)) return UT_SENSORSTATUS_TIMEOUT;
    uint16_t device_id = ((uint16_t)data[0] << 8) | data[1];
    if(device_id != 0x700) {
        FURI_LOG_E(
            APP_NAME,
            "Sensor %s returned wrong ID 0x%02X, expected 0x700",
            sensor->name,
            device_id);
        return false;
    }
    
    uint8_t config[1] = {0};
    
    // Soft Reset the sensor
    config[0] = 0b10000000;
    if(!unitemp_i2c_writeRegArray(i2c_sensor, 0x0E, 1, config)) return UT_SENSORSTATUS_TIMEOUT;
    furi_delay_ms(50);
    // Set the sample rate to 5Hz
    config[0] = 0b01110000;
    if(!unitemp_i2c_writeRegArray(i2c_sensor, 0x0E, 1, config)) return UT_SENSORSTATUS_TIMEOUT;
    // Enable Sensor Mode
    config[0] = 0b00000001;
    if(!unitemp_i2c_writeRegArray(i2c_sensor, 0x0F, 1, config)) return UT_SENSORSTATUS_TIMEOUT;

    return true;
}

bool unitemp_HDC2080_deinit(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;
    UNUSED(i2c_sensor);
    return true;
}

// Function helps read the data while keeping the code clean...
uint8_t getAddrVal(I2CSensor* sensor, uint8_t addr) {
    uint8_t data[1] = {addr};
    if(!unitemp_i2c_writeArray(sensor, 1, data)) return UT_SENSORSTATUS_TIMEOUT;
    furi_delay_ms(10);
    if(!unitemp_i2c_readArray(sensor, 1, data)) return UT_SENSORSTATUS_TIMEOUT;
    return data[0];
}
UnitempStatus unitemp_HDC2080_update(Sensor* sensor) {
    I2CSensor* i2c_sensor = (I2CSensor*)sensor->instance;

    uint8_t data[2] = {0};
    data[0] = getAddrVal(i2c_sensor, 0);
    data[1] = getAddrVal(i2c_sensor, 1);
    sensor->temp = ((float)(((uint16_t)data[1] << 8) | data[0]) / 65536) * 165 - (40.0+ 0.08*(3.3-1.8)); // 3.3v offset correction

    data[0] = getAddrVal(i2c_sensor, 2);
    data[1] = getAddrVal(i2c_sensor, 3);
    sensor->hum = ((float)(((uint16_t)data[1] << 8) | data[0]) / 65536) * 100;

    return UT_SENSORSTATUS_OK;
}
