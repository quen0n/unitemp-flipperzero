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
#include "unitemp_utils.h"

float unitemp_calculate_dew_point(float temperature_in_celsius, float humidity_in_percent) {
    if(humidity_in_percent <= 0.0f || humidity_in_percent > 100.0f ||
       temperature_in_celsius < -40.0f) {
        return -128.0f;
    }

    float a = 17.27f;
    float b = 237.7f;
    float alpha = ((a * temperature_in_celsius) / (b + temperature_in_celsius)) +
                  logf(humidity_in_percent / 100.0f);
    float dew_point = (b * alpha) / (a - alpha);

    return dew_point;
}

float unitemp_calculate_heat_index(float temperature_in_fahrenheit, float humidity_in_percent) {
    if(humidity_in_percent <= 0.0f || humidity_in_percent > 100.0f ||
       temperature_in_fahrenheit < 80.0f) {
        return temperature_in_fahrenheit;
    }

    float temp_f = temperature_in_fahrenheit;
    float rh = humidity_in_percent;

    // Rothfusz regression coefficients
    const float c1 = -42.379f;
    const float c2 = 2.04901523f;
    const float c3 = 10.14333127f;
    const float c4 = -0.22475541f;
    const float c5 = -0.00683783f;
    const float c6 = -0.05481717f;
    const float c7 = 0.00122874f;
    const float c8 = 0.00085282f;
    const float c9 = -0.00000199f;

    float t2 = temp_f * temp_f;
    float rh2 = rh * rh;

    float hi_f = c1 + c2 * temp_f + c3 * rh + c4 * temp_f * rh + c5 * t2 + c6 * rh2 +
                 c7 * t2 * rh + c8 * temp_f * rh2 + c9 * t2 * rh2;

    return hi_f;
}

float unitemp_convert_pa_to_mm_hg(float pressure_in_pa) {
    return pressure_in_pa * 0.007500638f;
}
float unitemp_convert_pa_to_in_hg(float pressure_in_pa) {
    return pressure_in_pa * 0.0002953007f;
}
float unitemp_convert_pa_to_kpa(float pressure_in_pa) {
    return pressure_in_pa / 1000.0f;
}
float unitemp_convert_pa_to_hpa(float pressure_in_pa) {
    return pressure_in_pa / 100.0f;
}
