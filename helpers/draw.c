#include "draw.h"
#include <stdlib.h>
#include <gui/elements.h>
#include <locale/locale.h>

#include "unitemp_icons.h"

#define TEMP_STR_SIZE 32
static char temp_str[TEMP_STR_SIZE];

void unitemp_draw_temperature(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    //Не рисовать, если координаты равны UT_DATA_POS_NONE
    if(x == 255 && y == 255) return;
    //Drawing a frame
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);
    canvas_draw_rframe(canvas, x, y, 54, 19, 3);

    float temperature = sensor->temperature;
    if(temperature_unit == UT_TEMP_FAHRENHEIT) {
        temperature = locale_celsius_to_fahrenheit(temperature);
    }
    int8_t temp_dec = abs((int16_t)(temperature * 10) % 10);

    //Drawing icon
    canvas_draw_icon(
        canvas,
        x + 3,
        y + 3,
        (temperature_unit == UT_TEMP_CELSIUS ? &I_temp_C_11x14 : &I_temp_F_11x14));

    if(temperature == -128.0f || sensor->status != UT_SENSORSTATUS_OK) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, "--");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 50, y + 10 + 3, AlignRight, AlignCenter, ". -");
        return;
    }

    //Whole part of temperature
    //A crutch for displaying the sign of a number less than 0
    uint8_t offset = 0;
    if(temperature < 0 && temperature > -1) {
        temp_str[0] = '-';
        offset = 1;
    }
    snprintf((char*)(temp_str + offset), TEMP_STR_SIZE, "%d", (int16_t)temperature);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 27 + ((temperature <= -10 || temperature > 99) ? 5 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);
    //Printing the fractional part of the temperature in the range from -9 to 99 (when there are two digits in the number)
    if(temperature > -10 && temperature <= 99) {
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", temp_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
}

void unitemp_draw_sensor_single(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    canvas_set_font(canvas, FontPrimary);

    const uint8_t max_width = 56;

    char sensor_name[11] = {0};
    memcpy(sensor_name, sensor->name, 10);

    if(canvas_string_width(canvas, sensor_name) > max_width) {
        uint8_t i = 10;
        while((canvas_string_width(canvas, sensor_name) > max_width - 6) && (i != 0)) {
            sensor_name[i--] = '\0';
        }
        sensor_name[++i] = '.';
        sensor_name[++i] = '.';
    }

    canvas_draw_str_aligned(canvas, x + 27, y + 3, AlignCenter, AlignCenter, sensor_name);
    unitemp_draw_temperature(canvas, sensor, temperature_unit, x, y + 8);
}
