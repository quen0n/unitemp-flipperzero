#include "sensors.h"
#include "unitemp.h"

#include "sensors/DHTxx.h"
#include "sensors/AM2320.h"
#include "./sensors/LM75.h"
//BMP280, BME280, BME680
#include "./sensors/BMx280.h"
#include "./sensors/BME680.h"
#include "./sensors/SHT30.h"
#include "./sensors/BMP180.h"
#include "./sensors/HTU21x.h"
#include "./sensors/HDC1080.h"
#include "./sensors/MAX31855.h"
#include "./sensors/MAX6675.h"
#include "./sensors/DS18x2x.h"

#define UPDATE_PERIOD_MS 250UL

static Sensor** sensors_list = NULL;
//Number of loaded sensors
static uint8_t sensors_count = 0;

//List of sensor models
static const SensorModel* sensor_model_list[] = {
    &AHT10, //tested
    &AM2320_SW, //do not work :|
    &AM2320_I2C, //tested
    &BMP180, //tested
    &BMP280,     &BME280,
    &BME680, //tested
    &Dallas, //tested
    &DHT11, //tested
    &DHT20, //tested
    &DHT21, //tested
    &DHT22,
    &GXHT30, //tested
    &HDC1080, //tested
    &HTU21x, //tested
    &LM75, //tested
    &MAX6675, //tested
    &MAX31855, //tested
    &SHT30, //tested

};
//Number of sensor models
#define SENSOR_MODELS_COUNT (int)(sizeof(sensor_model_list) / sizeof(const SensorModel*))

const SensorModel** unitemp_sensors_models_get(void) {
    return sensor_model_list;
}

uint8_t unitemp_sensors_models_get_count(void) {
    return SENSOR_MODELS_COUNT;
}

const SensorModel* unitemp_sensors_get_model_from_str(char* str) {
    if(str == NULL) return NULL;

    for(uint8_t i = 0; i < SENSOR_MODELS_COUNT; i++) {
        if(!strcmp(str, sensor_model_list[i]->modelname)) {
            return sensor_model_list[i];
        }
    }
    UNITEMP_DEBUG("Unknown sensor model: %s", str);
    return NULL;
}

/* Periodically requests measurements and reads temperature. This function runs in a separare thread. */
int32_t unitemp_sensors_update_callback(void* context) {
    furi_check(context);

    UnitempApp* app = context;
    for(;;) {
        for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
            unitemp_sensor_update((unitemp_sensors_get(i)), app);
        }

        const uint32_t flags =
            furi_thread_flags_wait(UnitempThreadFlagExit, FuriFlagWaitAny, UPDATE_PERIOD_MS);

        /* If an exit signal was received, return from this thread. */
        if(flags != (unsigned)FuriFlagErrorTimeout) break;
    }
    return 0;
}

Sensor* unitemp_sensor_alloc(char* name, const SensorModel* model, char* args) {
    if(name == NULL || model == NULL || args == NULL) return NULL;

    bool status = false;

    //Allocation of memory for the sensor
    Sensor* sensor = malloc(sizeof(Sensor));
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s allocation error", name);
        return NULL;
    }

    //Allocating memory for a name
    sensor->name = malloc(11);
    if(sensor->name == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor %s name allocation error", name);
        return NULL;
    }
    //Recording the sensor name
    strcpy(sensor->name, name);
    //Sensor model
    sensor->model = model;
    //Status sensor by default - error
    sensor->status = UT_SENSORSTATUS_ERROR;
    //Time of last poll
    sensor->lastPollingTime =
        furi_get_tick() - 10000; //so that the first survey occurs as early as possible

    sensor->temperature = -128.0f;
    sensor->humidity = -128.0f;
    sensor->pressure = -128.0f;
    sensor->temperature_offset = 0;
    //Memory allocation for a sensor instance depending on its interface
    status = sensor->model->interface->allocator(sensor, args);

    //Exit if the sensor is successfully deployed
    if(status) {
        UNITEMP_DEBUG("Sensor %s allocated", name);
        return sensor;
    }
    //Exit with clearing if memory for the sensor has not been allocated
    free(sensor->name);
    free(sensor);
    FURI_LOG_E(APP_NAME, "Sensor %s(%s) allocation error", name, model->modelname);
    return NULL;
}

void unitemp_sensor_free(Sensor* sensor) {
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Null pointer sensor releasing");
        return;
    }
    if(sensor->model == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor model is null");
        return;
    }
    if(sensor->model->mem_releaser == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor releaser is null");
        return;
    }
    bool status = false;
    //Freeing up memory for an instance
    status = sensor->model->interface->mem_releaser(sensor);

    if(status) {
        UNITEMP_DEBUG("Sensor %s memory successfully released", sensor->name);
    } else {
        FURI_LOG_E(APP_NAME, "Sensor %s memory is not released", sensor->name);
    }
    free(sensor->name);
    free(sensor);
}

void unitemp_sensors_add(Sensor* sensor) {
    furi_check(sensor);

    sensors_list = (Sensor**)realloc(sensors_list, (sensors_count + 1) * sizeof(Sensor*));
    sensors_list[sensors_count] = sensor;
    sensors_count++;
}

void unitemp_sensors_free(void) {
    for(uint8_t i = 0; i < sensors_count; i++) {
        unitemp_sensor_free(sensors_list[i]);
    }
    free(sensors_list);
    sensors_count = 0;
}

uint8_t unitemp_sensors_get_count(void) {
    if(sensors_list == NULL) return 0;
    return sensors_count;
}

bool unitemp_sensors_load(void) {
    UNITEMP_DEBUG("Loading sensors...");
    //Temperature offset
    int temp_offset = 0;
    Sensor* sensor;

    sensor = unitemp_sensor_alloc("DHT11 test", unitemp_sensors_get_model_from_str("DHT11"), "7");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor =
        unitemp_sensor_alloc("HTU21 test", unitemp_sensors_get_model_from_str("HTU21x"), "80");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor = unitemp_sensor_alloc("AMH10 tst", unitemp_sensors_get_model_from_str("AHT10"), "72");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor =
        unitemp_sensor_alloc("BMP180 tst", unitemp_sensors_get_model_from_str("BMP180"), "EE");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor =
        unitemp_sensor_alloc("BME680 tst", unitemp_sensors_get_model_from_str("BME680"), "ED");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor = unitemp_sensor_alloc("MAX6675", unitemp_sensors_get_model_from_str("MAX6675"), "6");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor = unitemp_sensor_alloc("MAX31855", unitemp_sensors_get_model_from_str("MAX31855"), "4");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor = unitemp_sensor_alloc(
        "Dallas", unitemp_sensors_get_model_from_str("Dallas"), "17 105D15B102080016");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    sensor = unitemp_sensor_alloc("DHT20 test", unitemp_sensors_get_model_from_str("DHT20"), "7D");
    if(sensor != NULL) {
        sensor->temperature_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }
    // sensor = unitemp_sensor_alloc("GXHT30", unitemp_sensors_get_model_from_str("GXHT30"), "88");
    // if(sensor != NULL) {
    //     sensor->temperature_offset = temp_offset;
    //     unitemp_sensors_add(sensor);
    // }
    // sensor = unitemp_sensor_alloc("HDC1080", unitemp_sensors_get_model_from_str("HDC1080"), "80");
    // if(sensor != NULL) {
    //     sensor->temperature_offset = temp_offset;
    //     unitemp_sensors_add(sensor);
    // }
    // sensor = unitemp_sensor_alloc("SHT30", unitemp_sensors_get_model_from_str("SHT30"), "88");
    // if(sensor != NULL) {
    //     sensor->temperature_offset = temp_offset;
    //     unitemp_sensors_add(sensor);
    // }
    // sensor = unitemp_sensor_alloc("LM75", unitemp_sensors_get_model_from_str("LM75"), "88");
    // if(sensor != NULL) {
    //     sensor->temperature_offset = temp_offset;
    //     unitemp_sensors_add(sensor);
    // }
    // sensor =
    //     unitemp_sensor_alloc("AM2320_I2C", unitemp_sensors_get_model_from_str("AM2320_I2C"), "B8");
    // if(sensor != NULL) {
    //     sensor->temperature_offset = temp_offset;
    //     unitemp_sensors_add(sensor);
    // }
    // sensor = unitemp_sensor_alloc("AM2320_SW", unitemp_sensors_get_model_from_str("AM2320"), "14");
    // if(sensor != NULL) {
    //     sensor->temperature_offset = temp_offset;
    //     unitemp_sensors_add(sensor);
    // }

    FURI_LOG_I(APP_NAME, "Sensors loaded successfully. Total: %d", sensors_count);
    return true;
}

bool unitemp_sensors_init(void* context) {
    if(context == NULL) return false;
    UnitempApp* app = context;

    bool result = true;

    //Searching through sensors from the list
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        //Turning on 5V if there is none on port 1 FZ
        //May disappear when USB is disconnected
        if(app->settings->otg_auto_on && !power_is_otg_enabled(app->power)) {
            power_enable_otg(app->power, true);
        }

        if(!(*sensors_list[i]->model->initializer)(sensors_list[i])) {
            FURI_LOG_E(
                APP_NAME,
                "An error occurred during sensor initialization %s",
                sensors_list[i]->name);
            result = false;
        } else {
            FURI_LOG_I(APP_NAME, "Sensor %s successfully initialized", sensors_list[i]->name);
        }
    }
    return result;
}

bool unitemp_sensors_deinit(void* context) {
    if(context == NULL) return false;
    UnitempApp* app = context;

    bool result = true;

    //Turning off 5 V if it was not turned on before
    power_enable_otg(app->power, app->settings->otg_latest_state);

    //Searching through sensors from the list
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        if(!(*sensors_list[i]->model->deinitializer)(sensors_list[i])) {
            FURI_LOG_E(
                APP_NAME,
                "An error occurred during sensor deinitialization %s",
                sensors_list[i]->name);
            result = false;
        } else {
            FURI_LOG_I(APP_NAME, "Sensor %s successfully deinitialized", sensors_list[i]->name);
        }
    }

    return result;
}

Sensor* unitemp_sensors_get(uint8_t index) {
    return sensors_list[index];
}

SensorStatus unitemp_sensor_update(Sensor* sensor, void* context) {
    if(sensor == NULL || context == NULL) {
        return UT_SENSORSTATUS_ERROR;
    }
    if(sensor->status == UT_SENSORSTATUS_INACTIVE) {
        return UT_SENSORSTATUS_INACTIVE;
    }
    UnitempApp* app = context;

    //Checking the validity of the sensor polling
    if(furi_get_tick() - sensor->lastPollingTime < sensor->model->polling_interval) {
        //Return an error if the last sensor poll was unsuccessful
        if(sensor->status == UT_SENSORSTATUS_TIMEOUT) {
            return UT_SENSORSTATUS_TIMEOUT;
        }
        return UT_SENSORSTATUS_EARLYPOOL;
    }

    sensor->lastPollingTime = furi_get_tick();

    if(app->settings->otg_auto_on && !power_is_otg_enabled(app->power)) {
        power_enable_otg(app->power, true);
    }

    sensor->status = sensor->model->interface->updater(sensor);

    if(sensor->status != UT_SENSORSTATUS_OK && sensor->status != UT_SENSORSTATUS_POLLING) {
        FURI_LOG_W(APP_NAME, "Sensor %s update status %d", sensor->name, sensor->status);
    } else {
        UNITEMP_DEBUG(
            "Sensor %s successfully updated. Values: temp=%.2f, hum=%.2f",
            sensor->name,
            (double)sensor->temperature,
            (double)sensor->humidity);
    }

    if(sensor->status == UT_SENSORSTATUS_OK) {
        sensor->temperature += sensor->temperature_offset / 10.f;
    }
    return sensor->status;
}
