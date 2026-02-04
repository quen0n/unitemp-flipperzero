#include "sensors.h"
#include "unitemp.h"

#include "sensors/DHTxx.h"
#include "sensors/AM2320.h"

//TODO: Перенести всё что относится к GPIO в другой файл
//List of available GPIO pins with their numbers and names
#define GPIO_ITEMS 13U
static const SensorGpioPin gpio_list[] = {
    {2, "2 (A7)", &gpio_ext_pa7},
    {3, "3 (A6)", &gpio_ext_pa6},
    {4, "4 (A4)", &gpio_ext_pa4},
    {5, "5 (B3)", &gpio_ext_pb3},
    {6, "6 (B2)", &gpio_ext_pb2},
    {7, "7 (C3)", &gpio_ext_pc3},
    {10, "10 (SWC)", &gpio_swclk},
    {12, "12 (SIO)", &gpio_swdio},
    {13, "13 (TX)", &gpio_usart_tx},
    {14, "14 (RX)", &gpio_usart_rx},
    {15, "15 (C1)", &gpio_ext_pc1},
    {16, "16 (C0)", &gpio_ext_pc0},
    {17, "17 (1W)", &gpio_ibutton}};

static Sensor** sensors_list = NULL;
//Number of loaded sensors
static uint8_t sensors_count = 0;

//List of sensor models
static const SensorModel* sensor_model_list[] = {
    &DHT11,
    &DHT21,
    &DHT22,
    &AM2320_SW,
};
//Number of sensor models
#define SENSOR_TYPES_COUNT (int)(sizeof(sensor_model_list) / sizeof(const SensorModel*))

const SensorModel* unitemp_sensors_get_model_from_str(char* str) {
    UNUSED(str);
    if(str == NULL) return NULL;
    for(uint8_t i = 0; i < SENSOR_TYPES_COUNT; i++) {
        if(!strcmp(str, sensor_model_list[i]->modelname)) {
            return sensor_model_list[i];
        }
    }
    return NULL;
}

//List of interfaces that are attached to GPIO (defined by index)
//NULL - port is free, pointer to interface - port is occupied by this interface
static const SensorConnectionInterface* gpio_interfaces_list[GPIO_ITEMS] = {0};

const SensorGpioPin* unitemp_gpio_get_from_int(uint8_t number) {
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(gpio_list[i].num == number) {
            return &gpio_list[i];
        }
    }
    return NULL;
}

uint8_t unitemp_gpio_to_index(const GpioPin* gpio) {
    if(gpio == NULL) return 255;
    for(uint8_t i = 0; i < GPIO_ITEMS; i++) {
        if(gpio_list[i].pin->pin == gpio->pin && gpio_list[i].pin->port == gpio->port) {
            return i;
        }
    }
    return 255;
}

void unitemp_gpio_lock(const SensorGpioPin* gpio, const SensorConnectionInterface* interface) {
    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = interface;
}

void unitemp_gpio_unlock(const SensorGpioPin* gpio) {
    uint8_t i = unitemp_gpio_to_index(gpio->pin);
    if(i == 255) return;
    gpio_interfaces_list[i] = NULL;
}

Sensor* unitemp_sensor_alloc(char* name, const SensorModel* type, char* args) {
    if(name == NULL || type == NULL) return NULL;
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
    //Sensor type
    sensor->type = type;
    //Status sensor by default - error
    sensor->status = UT_SENSORSTATUS_ERROR;
    //Time of last poll
    sensor->lastPollingTime =
        furi_get_tick() - 10000; //so that the first survey occurs as early as possible

    sensor->temp = -128.0f;
    sensor->hum = -128.0f;
    sensor->pressure = -128.0f;
    sensor->temp_offset = 0;
    //Memory allocation for a sensor instance depending on its interface
    status = sensor->type->interface->allocator(sensor, args);

    //Exit if the sensor is successfully deployed
    if(status) {
        UNITEMP_DEBUG("Sensor %s allocated", name);
        return sensor;
    }
    //Exit with clearing if memory for the sensor has not been allocated
    free(sensor->name);
    free(sensor);
    FURI_LOG_E(APP_NAME, "Sensor %s(%s) allocation error", name, type->modelname);
    return NULL;
}

void unitemp_sensor_free(Sensor* sensor) {
    if(sensor == NULL) {
        FURI_LOG_E(APP_NAME, "Null pointer sensor releasing");
        return;
    }
    if(sensor->type == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor type is null");
        return;
    }
    if(sensor->type->mem_releaser == NULL) {
        FURI_LOG_E(APP_NAME, "Sensor releaser is null");
        return;
    }
    bool status = false;
    //Freeing up memory for an instance
    status = sensor->type->interface->mem_releaser(sensor);

    if(status) {
        UNITEMP_DEBUG("Sensor %s memory successfully released", sensor->name);
    } else {
        FURI_LOG_E(APP_NAME, "Sensor %s memory is not released", sensor->name);
    }
    free(sensor->name);
    free(sensor);
}

void unitemp_sensors_add(Sensor* sensor) {
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
    //Sensor name
    char name[11] = "DHT11 test";
    //Sensor model
    char model[11] = "DHT11";
    //Temperature offset
    int temp_offset = 0;
    //Name length limit
    name[10] = '\0';

    Sensor* sensor = unitemp_sensor_alloc(name, unitemp_sensors_get_model_from_str(model), "7");
    if(sensor != NULL) {
        sensor->temp_offset = temp_offset;
        unitemp_sensors_add(sensor);
    }

    FURI_LOG_I(APP_NAME, "Sensors loaded successfully. Total: %d", sensors_count);
    return true;
}

bool unitemp_sensors_init(void* ctx) {
    bool result = true;

    UnitempApp* app = (UnitempApp*)ctx;

    //Searching through sensors from the list
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        //Turning on 5V if there is none on port 1 FZ
        //May disappear when USB is disconnected
        power_enable_otg(app->power, true);
        if(!(*sensors_list[i]->type->initializer)(sensors_list[i])) {
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

bool unitemp_sensors_deinit(void* ctx) {
    bool result = true;

    UnitempApp* app = (UnitempApp*)ctx;
    //Turning off 5 V if it was not turned on before
    power_enable_otg(app->power, false);

    //Searching through sensors from the list
    for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
        if(!(*sensors_list[i]->type->deinitializer)(sensors_list[i])) {
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

Sensor** unitemp_sensors_get(void) {
    return sensors_list;
}

SensorStatus unitemp_sensor_update(Sensor* sensor, void* ctx) {
    UnitempApp* app = (UnitempApp*)ctx;
    if(sensor == NULL) {
        return UT_SENSORSTATUS_ERROR;
    }

    if(sensor->status == UT_SENSORSTATUS_INACTIVE) {
        return UT_SENSORSTATUS_INACTIVE;
    }

    //Checking the validity of the sensor polling
    if(furi_get_tick() - sensor->lastPollingTime < sensor->type->pollingInterval) {
        //Return an error if the last sensor poll was unsuccessful
        if(sensor->status == UT_SENSORSTATUS_TIMEOUT) {
            return UT_SENSORSTATUS_TIMEOUT;
        }
        return UT_SENSORSTATUS_EARLYPOOL;
    }

    sensor->lastPollingTime = furi_get_tick();

    power_enable_otg(app->power, false);

    sensor->status = sensor->type->interface->updater(sensor);

    if(sensor->status != UT_SENSORSTATUS_OK && sensor->status != UT_SENSORSTATUS_POLLING) {
        UNITEMP_DEBUG("Sensor %s update status %d", sensor->name, sensor->status);
    }

    if(sensor->status == UT_SENSORSTATUS_OK) {
        sensor->temp += sensor->temp_offset / 10.f;
    }

    return sensor->status;
}
