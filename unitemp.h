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
#include <gui/view.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>

#include "scenes/unitemp_scene.h"

#include "views/view_no_sensors.h"
#include "views/view_single_sensor.h"

#include <power/power_service/power.h>

#include <storage/storage.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "sensors.h"

/* Declaring Macro Substitutions */
//Application name
#define APP_NAME        "Unitemp"
//Application version
#define UNITEMP_APP_VER "2.0.0-dev"

//Settings file name
#define APP_SETTINGS_FILENAME "settings.cfg"
#define APP_SETTINGS_VERSION  (1)

//Text buffer size
#define TEXT_STORE_SIZE 32

//Debug macro (only if app builded in debug mode)
#ifdef FURI_DEBUG
#define UNITEMP_DEBUG(msg, ...) FURI_LOG_D(APP_NAME, msg, ##__VA_ARGS__)
#else
#define UNITEMP_DEBUG(msg, ...)
#endif

typedef enum {
    UnitempViewSubmenu,
    UnitempViewPopup,
    UnitempViewWidget,
    UnitempViewVariableList,
    UnitempViewNoSensors,
    UnitempViewSingleSensor,
} UnitempView;

//Temperature units
typedef enum {
    UT_TEMP_CELSIUS,
    UT_TEMP_FAHRENHEIT,

    UT_TEMP_COUNT
} TempMeasureUnit;

//Pressure units
typedef enum {
    UT_PRESSURE_MM_HG,
    UT_PRESSURE_IN_HG,
    UT_PRESSURE_KPA,
    UT_PRESSURE_HPA,

    UT_PRESSURE_COUNT
} PressureMeasureUnit;

// Humidity units
typedef enum {
    UT_HUMIDITY_RELATIVE, // Relative humidity
    UT_HUMIDITY_DEW_POINT, // Dew point

    UT_HUMIDITY_COUNT // Number of humidity modes
} HumidityMeausureUnit;

/* Declaration of structures */
//Plugin settings
typedef struct {
    //Endless backlight operation
    bool infinity_backlight;
    //Temperature unit
    TempMeasureUnit temperature_unit;
    // Humidity units
    HumidityMeausureUnit humidity_unit;
    //Pressure unit
    PressureMeasureUnit pressure_unit;
    // Do calculate and show heat index
    bool heat_index;
    //Latest OTG status
    bool last_otg_state;
} UnitempSettings;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Popup* popup;
    Widget* widget;
    Submenu* submenu;
    VariableItemList* var_item_list;
    NoSensors* no_sensors;
    SingleSensor* single_sensor;
    Gui* gui;

    Sensor** sensors_list;

    Storage* storage;
    File* file;
    NotificationApp* notifications;

    FuriThread* reader_thread;
    Power* power;

    //TODO: переместить список загруженных датчиков в эту структуру
    UnitempSettings* settings;
} UnitempApp;

/* Flags which the reader thread responds to */
typedef enum {
    UnitempThreadFlagExit = 1,
} UnitempThreadFlag;

void unitemp_submenu_callback(void* context, uint32_t index);
void unitemp_widget_callback(GuiButtonType result, InputType type, void* context);

bool unitemp_settings_load(void* context);
bool unitemp_settings_save(void* context);

/**
 * @brief Calculates the dew point temperature based on ambient temperature and humidity.
 *
* Computes the dew point temperature using the provided ambient temperature
 * and relative humidity values. The dew point is the temperature at which
 * water vapor in the air condenses into liquid water.
 * 
 * @param temperature_in_celsius The ambient air temperature in degrees Celsius.
 * @param humidity_in_percent The relative humidity in percent (0-100).
 * 
 * @return The calculated dew point temperature in degrees Celsius.
 * 
 * @note This function uses an empirical approximation and is suitable for
 *       temperatures in the range of -40°C to +50°C and humidity from 1% to 100%.
 *
 * @see https://en.wikipedia.org/wiki/Dew_point
 */
float unitemp_calculate_dew_point(float temperature_in_celsius, float humidity_in_percent);

/**
 * @brief Calculates the heat index based on temperature and humidity.
 * 
 * The heat index is a measure of how hot it actually feels when relative humidity
 * is factored in with the actual air temperature. This function uses the standard
 * heat index formula to compute the apparent temperature.
 * 
 * @param temperature_in_fahrenheit The ambient air temperature in degrees Fahrenheit.
 * @param humidity_in_percent The relative humidity as a percentage (0-100).
 * 
 * @return The calculated heat index value in degrees Fahrenheit.
 * 
 * @note The heat index is typically defined for temperatures above 80°F (26.7°C).
 *       For lower temperatures, the function may still return a value but it should
 *       not be relied upon for accuracy.
 * 
 * @see https://www.ncei.noaa.gov/products/heat-index for more information about heat index calculation.
 */
float unitemp_calculate_heat_index(float temperature_in_fahrenheit, float humidity_in_percent);

/**
 * @brief Convert pressure from Pascals to millimeters of mercury
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in mmHg
 */
float unitemp_calculate_pa_to_mm_hg(float pressure_in_pa);

/**
 * @brief Convert pressure from Pascals to inches of mercury
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in inHg
 */
float unitemp_calculate_pa_to_in_hg(float pressure_in_pa);

/**
 * @brief Convert pressure from Pascals to kilopascals
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in kPa
 */
float unitemp_calculate_pa_to_kpa(float pressure_in_pa);

/**
 * @brief Convert pressure from Pascals to hectopascals
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in hPa
 */
float unitemp_calculate_pa_to_hpa(float pressure_in_pa);

#endif //UNITEMP_H_
