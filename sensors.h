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
#ifndef UNITEMP_SENSORS_H_
#define UNITEMP_SENSORS_H_

#include <furi.h>
#include <furi_hal.h>

#include "../interfaces/singlewire.h"

// Values returned when polling the sensor
typedef enum {
    UT_DATA_TYPE_TEMP,
    UT_DATA_TYPE_TEMP_HUM,
    UT_DATA_TYPE_TEMP_PRESS,
    UT_DATA_TYPE_TEMP_HUM_PRESS,
    UT_DATA_TYPE_TEMP_HUM_CO2,

    UT_DATA_TYPE_COUNT
} SensorDataType;

//Return Types
typedef enum {
    UT_SENSORSTATUS_OK, //Everything is fine, the polling is successful
    UT_SENSORSTATUS_TIMEOUT, //The sensor did not respond
    UT_SENSORSTATUS_EARLYPOOL, //Poll before the required delay
    UT_SENSORSTATUS_BADCRC, //Invalid checksum
    UT_SENSORSTATUS_ERROR, //Other errors
    UT_SENSORSTATUS_POLLING, //A transformation occurs in the sensor
    UT_SENSORSTATUS_INACTIVE, //The sensor is being edited or deleted
} SensorStatus;

//Unitemp GPIO Pin structure
typedef struct SensorGpioPin {
    const uint8_t num; //Pin number (2-7, 10, 12-17)
    const char* name; //Pin name from Flipper Zero shell
    const GpioPin* pin; //Pointer to GPIO pin structure from Furi HAL
} SensorGpioPin;

typedef struct Sensor Sensor;

/**
 * @brief Function pointer to allocate memory and prepare a sensor instance
 */
typedef bool(SensorAllocator)(Sensor* sensor, char* args);
/**
 * @brief Pointer to the sensor memory release function
 */
typedef bool(SensorFree)(Sensor* sensor);
/**
 * @brief Sensor initialization function pointer
 */
typedef bool(SensorInitializer)(Sensor* sensor);
/**
 * @brief Pointer to the sensor deinitialization function
 */
typedef bool(SensorDeinitializer)(Sensor* sensor);
/**
 * @brief Pointer to the sensor value update function
 */
typedef SensorStatus(SensorUpdater)(Sensor* sensor);

//Sensor connection interface structure
typedef struct SensorConnectionInterface {
    //Interface name
    const char* name;
    //Interface memory allocation function
    SensorAllocator* allocator;
    //Interface memory release function
    SensorFree* mem_releaser;
    //Sensor value update function via interface
    SensorUpdater* updater;
} SensorConnectionInterface;

//Sensor types
typedef struct {
    //Sensor model
    const char* modelname;
    //Full name with analogues
    const char* altname;
    //Return type
    SensorDataType data_type;
    //Connection interface
    const SensorConnectionInterface* interface;
    //Sensor polling interval (ms)
    uint16_t polling_interval;
    //Sensor memory allocation function
    SensorAllocator* allocator;
    //Sensor memory release function
    SensorFree* mem_releaser;
    //Sensor initialization function
    SensorInitializer* initializer;
    //Sensor deinitialization function
    SensorDeinitializer* deinitializer;
    //Sensor value update function
    SensorUpdater* updater;
} SensorModel;

//Sensor
typedef struct Sensor {
    //Sensor user name
    char* name;
    //Temperature
    float temperature;
    //Relative humidity
    float humidity;
    //Atmospheric pressure
    float pressure;
    //CO2 concentration
    float co2;
    //Temperature offset (x10)
    int8_t temperature_offset;
    //Sensor type
    const SensorModel* model;
    //Last sensor poll status
    SensorStatus status;
    //Time of the last sensor poll
    uint32_t lastPollingTime;
    //Sensor instance
    void* instance;
} Sensor;

/**
 * @brief Converting the port number on the FZ case to SensorGpioPin
 * @param name Port number on the FZ case
 * @return Pointer to SensorGpioPin on success, NULL on error
 */
const SensorGpioPin* unitemp_gpio_get_from_int(uint8_t number);
/**
 * @brief Locking GPIO by specified interface
 * @param gpio Pointer to port
 * @param interface Pointer to the interface on which the port will be occupied
 */
void unitemp_gpio_lock(const SensorGpioPin* gpio, const SensorConnectionInterface* interface);

/**
 * @brief Unlocking the port
 * @param gpio Pointer to port
 */
void unitemp_gpio_unlock(const SensorGpioPin* gpio);

/**
 * @brief Memory allocation for sensor
 * 
 * @param name Sensor name
 * @param type Sensor type
 * @param args Pointer to a string with sensor parameters
 * @return Pointer to the sensor in case of successful memory allocation, NULL on error
 */
Sensor* unitemp_sensor_alloc(char* name, const SensorModel* type, char* args);

/**
 * @brief Freeing up the memory of a specific sensor
 * @param sensor Pointer to sensor
 */
void unitemp_sensor_free(Sensor* sensor);

/**
 * @brief Add sensor to general list
 * @param sensor Pointer to sensor
 */
void unitemp_sensors_add(Sensor* sensor);

/**
 * @brief Freeing up the memory of all sensors
 */
void unitemp_sensors_free(void);

/**
 * @brief Get number of loaded sensors
 * @return Number of sensors
 */
uint8_t unitemp_sensors_get_count(void);
/**
 * @brief Loading sensors from SD card
 * @return true if the upload was successful
 */
bool unitemp_sensors_load();

/**
 * @brief Initializing loaded sensors
 * @param ctx Pointer to application context
 * @return true if everything went well
 */
bool unitemp_sensors_init(void* ctx);

/**
 * @brief Deinitialize loaded sensors
 * @return true if everything went well
 */
bool unitemp_sensors_deinit(void* ctx);

/**
 * @brief Update the data of the specified sensor
 * @param sensor Pointer to sensor
 * @param ctx Pointer to application context
 * @return Sensor poll status
 */
SensorStatus unitemp_sensor_update(Sensor* sensor, void* ctx);

Sensor** unitemp_sensors_get(void);

/**
* @brief Get a list of available sensor types
* @return Pointer to a list of sensors
*/
const SensorModel** unitemp_sensors_models_get(void);
uint8_t unitemp_sensors_models_get_count(void);

int32_t unitemp_sensors_update_callback(void* ctx);

#endif // UNITEMP_SENSORS
