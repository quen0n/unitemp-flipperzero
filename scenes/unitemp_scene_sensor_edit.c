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

#include "../unitemp.h"
#include "sensors.h"
#include "./interfaces/i2c_sensor.h"
#include "./interfaces/onewire_sensor.h"
#include "./interfaces/singlewire_sensor.h"
#include "./interfaces/spi_sensor.h"
#include "scenes/unitemp_scene.h"

#define OFFSET_BUFF_SIZE 5
static char* offset_buff;

static void _name_change_callback(VariableItem* item) {
    //Sensor* sensor = variable_item_get_context(item);
    variable_item_set_current_value_index(item, 0);
}

static void _offset_change_callback(VariableItem* item) {
    Sensor* sensor = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    sensor->temperature_offset = index - 20;
    snprintf(
        offset_buff,
        OFFSET_BUFF_SIZE,
        sensor->temperature_offset == 0 ? "%1.1f" : "%+1.1f",
        (double)(sensor->temperature_offset / 10.0));
    variable_item_set_current_value_text(item, offset_buff);
}

static void _gpio_change_callback(VariableItem* item) {
    Sensor* sensor = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    const SensorGpioPin* gpio_pin = NULL;
    const SensorConnectionInterface* interface = sensor->model->interface;
    if(interface == &unitemp_1w) {
        gpio_pin = ((OneWireSensor*)sensor->instance)->bus->bus_pin;
    } else if(interface == &singlewire) {
        gpio_pin = ((SingleWireSensor*)sensor->instance)->data_pin;
    } else if(interface == &unitemp_spi) {
        gpio_pin = ((SPISensor*)sensor->instance)->cs_pin;
    }
    gpio_pin = unitemp_gpio_get_aviable_pin(interface, index, gpio_pin);

    if(interface == &singlewire) {
        SingleWireSensor* instance = sensor->instance;
        instance->data_pin = gpio_pin;
        variable_item_set_current_value_text(item, instance->data_pin->name);
    } else if(interface == &unitemp_spi) {
        SPISensor* instance = sensor->instance;
        instance->cs_pin = unitemp_gpio_get_aviable_pin(interface, index, instance->cs_pin);
        variable_item_set_current_value_text(item, instance->cs_pin->name);
    } else if(interface == &unitemp_1w) {
        OneWireSensor* instance = sensor->instance;
        instance->bus->bus_pin =
            unitemp_gpio_get_aviable_pin(interface, index, instance->bus->bus_pin);
        variable_item_set_current_value_text(item, instance->bus->bus_pin->name);
    }
}

// bool _onewire_id_exist(uint8_t* id) {
//     if(id == NULL) return false;
//     for(uint8_t i = 0; i < unitemp_sensors_get_count(); i++) {
//         if(unitemp_sensors_get(i)->model->interface == &unitemp_1w) {
//             if(unitemp_onewire_id_compare(
//                    id, ((OneWireSensor*)(unitemp_sensors_get(i)->instance))->deviceID)) {
//                 return true;
//             }
//         }
//     }
//     return false;
// }

// static void _onewire_scan(OneWireSensor* ow_sensor) {
//     UNITEMP_DEBUG(
//         "devices on wire %d: %d", ow_sensor->bus->bus_pin->num, ow_sensor->bus->devices_count);

//     //One wire bus scan
//     unitemp_onewire_bus_init(ow_sensor->bus);
//     uint8_t* id = NULL;
//     do {
//         id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
//     } while(_onewire_id_exist(id));

//     if(id == NULL) {
//         unitemp_onewire_bus_enum_init();
//         id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
//         if(_onewire_id_exist(id)) {
//             do {
//                 id = unitemp_onewire_bus_enum_next(ow_sensor->bus);
//             } while(_onewire_id_exist(id) && id != NULL);
//         }
//         if(id == NULL) {
//             memset(ow_sensor->deviceID, 0, 8);
//             ow_sensor->family_code = 0;
//             unitemp_onewire_bus_deinit(ow_sensor->bus);
//             //TODO: чо за костыли и ваще чозабретто
//             //variable_item_set_current_value_text(onewire_addr_item, "empty");
//             //variable_item_set_current_value_text(
//             //  onewire_type_item, unitemp_onewire_sensor_getModel(editable_sensor));
//             return;
//         }
//     }

//     unitemp_onewire_bus_deinit(ow_sensor->bus);

//     memcpy(ow_sensor->deviceID, id, 8);
//     ow_sensor->family_code = id[0];

//     UNITEMP_DEBUG(
//         "Found sensor's ID: %02X%02X%02X%02X%02X%02X%02X%02X",
//         id[0],
//         id[1],
//         id[2],
//         id[3],
//         id[4],
//         id[5],
//         id[6],
//         id[7]);

//     if(ow_sensor->family_code != 0) {
//         char id_buff[10];
//         snprintf(
//             id_buff,
//             10,
//             "%02X%02X%02X",
//             ow_sensor->deviceID[1],
//             ow_sensor->deviceID[2],
//             ow_sensor->deviceID[3]);
//         //And it doesn’t climb anymore(
//         //TODO: чо за костыли
//         //variable_item_set_current_value_text(onewire_addr_item, id_buff);
//     } else {
//         //TODO: чо за костыли
//         //variable_item_set_current_value_text(onewire_addr_item, "empty");
//     } //TODO: чо за костыли
//     //variable_item_set_current_value_text(
//     //onewire_type_item, unitemp_onewire_sensor_getModel(editable_sensor));
// }

// static void _onwire_addr_change_callback(VariableItem* item) {
//     Sensor* sensor = variable_item_get_context(item);
//     variable_item_set_current_value_index(item, 0);
//     _onewire_scan(sensor->instance);
// }

static void _i2c_addr_change_callback(VariableItem* item) {
    Sensor* sensor = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    ((I2CSensor*)sensor->instance)->current_i2c_adress =
        ((I2CSensor*)sensor->instance)->min_i2c_adress + index * 2;
    char buff[5];
    snprintf(buff, 5, "0x%2X", ((I2CSensor*)sensor->instance)->current_i2c_adress >> 1);
    variable_item_set_current_value_text(item, buff);
}

void unitemp_scene_sensor_edit_on_enter(void* context) {
    UnitempApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    Sensor* sensor = app->editable_sensor;
    if(sensor == NULL) {
        UNITEMP_DEBUG("Editable sensor is NULL!");
        return;
    }

    offset_buff = malloc(OFFSET_BUFF_SIZE);

    variable_item_list_set_selected_item(var_item_list, 0);

    //Sensor model (not editable)
    item = variable_item_list_add(var_item_list, "Model", 1, NULL, NULL);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(
        item,
        (sensor->model->interface == &unitemp_1w ? unitemp_onewire_sensor_get_fc_name(sensor) :
                                                   sensor->model->modelname));

    //Sensor name
    item = variable_item_list_add(
        var_item_list, "Name", strlen(sensor->name) > 7 ? 1 : 2, _name_change_callback, sensor);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, sensor->name);

    char buff[11];
    //Device address on the I2C bus (for I2C sensors)
    if(sensor->model->interface == &unitemp_i2c) {
        item = variable_item_list_add(
            var_item_list,
            "I2C address",
            (((I2CSensor*)sensor->instance)->max_i2c_adress >> 1) -
                (((I2CSensor*)sensor->instance)->min_i2c_adress >> 1) + 1,
            _i2c_addr_change_callback,
            sensor);
        snprintf(buff, 5, "0x%2X", ((I2CSensor*)sensor->instance)->current_i2c_adress >> 1);
        variable_item_set_current_value_index(
            item,
            (((I2CSensor*)sensor->instance)->current_i2c_adress >> 1) -
                (((I2CSensor*)sensor->instance)->min_i2c_adress >> 1));
        variable_item_set_current_value_text(item, buff);
    } else if(
        sensor->model->interface == &unitemp_1w || sensor->model->interface == &singlewire ||
        sensor->model->interface == &unitemp_spi) {
        //Sensor connection port (for one wire, SPI and single wire)
        const SensorGpioPin* gpio_pin = NULL;
        if(sensor->model->interface == &unitemp_1w) {
            gpio_pin = ((OneWireSensor*)sensor->instance)->bus->bus_pin;
        } else if(sensor->model->interface == &singlewire) {
            gpio_pin = ((SingleWireSensor*)sensor->instance)->data_pin;
            unitemp_gpio_unlock(gpio_pin);
        } else if(sensor->model->interface == &unitemp_spi) {
            gpio_pin = ((SPISensor*)sensor->instance)->cs_pin;
            unitemp_gpio_unlock(gpio_pin);
        }

        uint8_t aviable_gpio_count =
            unitemp_gpio_get_aviable_pin_count(sensor->model->interface, gpio_pin);
        UNITEMP_DEBUG("aviable %d values", aviable_gpio_count);
        item = variable_item_list_add(
            var_item_list,
            sensor->model->interface == &unitemp_spi ? "CS pin" : "Data pin",
            aviable_gpio_count,
            _gpio_change_callback,
            sensor);

        uint8_t gpio_index = 0;
        for(uint8_t i = 0; i < aviable_gpio_count; i++) {
            if(unitemp_gpio_get_aviable_pin(sensor->model->interface, i, gpio_pin) == gpio_pin) {
                gpio_index = i;
                break;
            }
        }
        variable_item_set_current_value_index(item, gpio_index);
        variable_item_set_current_value_text(item, gpio_pin->name);
    }

    // // Device address on the one wire bus (for one wire sensors)
    // if(sensor->model->interface == &unitemp_1w) {
    //     item = variable_item_list_add(
    //         var_item_list, "Address", 2, _onwire_addr_change_callback, sensor);
    //     OneWireSensor* ow_sensor = sensor->instance;
    //     if(ow_sensor->family_code == 0) {
    //         variable_item_set_current_value_text(item, "Scan");
    //     } else {
    //         snprintf(
    //             buff,
    //             10,
    //             "%02X%02X%02X",
    //             ow_sensor->deviceID[1],
    //             ow_sensor->deviceID[2],
    //             ow_sensor->deviceID[3]);
    //         variable_item_set_current_value_text(item, buff);
    //     }
    // }

    //Temperature offset
    item =
        variable_item_list_add(var_item_list, "Temp. offset", 41, _offset_change_callback, sensor);
    variable_item_set_current_value_index(item, sensor->temperature_offset + 20);

    snprintf(
        offset_buff,
        OFFSET_BUFF_SIZE,
        sensor->temperature_offset == 0 ? "%1.1f" : "%+1.1f",
        (double)(sensor->temperature_offset / 10.0));
    variable_item_set_current_value_text(item, offset_buff);

    view_dispatcher_switch_to_view(app->view_dispatcher, UnitempViewVariableList);
}

bool unitemp_scene_sensor_edit_on_event(void* context, SceneManagerEvent event) {
    UnitempApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, UnitempSceneSensorEdit, event.event);
        consumed = true;
        // if(event.event == SubmenuIndexAddNewSensor) {
        //     // scene_manager_next_scene(app->scene_manager, UnitempSceneAddNewSensor);
        // } else if(event.event == SubmenuIndexSettings) {
        //     scene_manager_next_scene(app->scene_manager, UnitempSceneSettings);
        // } else if(event.event == SubmenuIndexHelp) {
        //     scene_manager_next_scene(app->scene_manager, UnitempSceneHelp);
        // } else if(event.event == SubmenuIndexAbout) {
        //     scene_manager_next_scene(app->scene_manager, UnitempSceneAbout);
        // }
    }

    return consumed;
}

void unitemp_scene_sensor_edit_on_exit(void* context) {
    UnitempApp* app = context;
    variable_item_list_reset(app->var_item_list);
    free(offset_buff);
    if(app->editable_sensor != NULL) {
        unitemp_sensor_free(app->editable_sensor);
    }
}
