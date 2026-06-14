/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk
*/
#ifndef UNITEMP_MHZ19C_PWM_H
#define UNITEMP_MHZ19C_PWM_H

#include "../sensors.h"
#include <stdbool.h>

extern const SensorModel MHZ19C_PWM;
extern const SensorConnectionInterface unitemp_mhz19c_pwm;

/* True when no PWM edge has been detected for ≥5 s after first valid tick. */
bool mhz19c_pwm_is_frozen(Sensor* sensor);

/* Request a soft reset: clears averaging buffer and CO2 on next update. */
void mhz19c_pwm_request_reset(Sensor* sensor);

/* PWM averaging window (1..30 samples). */
uint8_t mhz19c_pwm_get_avg(Sensor* sensor);
void mhz19c_pwm_set_avg(Sensor* sensor, uint8_t avg);

/* Sensor detection range used as the PWM scale (2000..10000 ppm). */
uint16_t mhz19c_pwm_get_range(Sensor* sensor);
void mhz19c_pwm_set_range(Sensor* sensor, uint16_t range_ppm);

/* Sound alert threshold (800..5000 ppm). */
uint16_t mhz19c_pwm_get_alert(Sensor* sensor);
void mhz19c_pwm_set_alert(Sensor* sensor, uint16_t alert_ppm);

/* Per-sensor LED / sound alert switches. */
bool mhz19c_pwm_get_led(Sensor* sensor);
void mhz19c_pwm_set_led(Sensor* sensor, bool enabled);
bool mhz19c_pwm_get_sound(Sensor* sensor);
void mhz19c_pwm_set_sound(Sensor* sensor, bool enabled);

#endif
