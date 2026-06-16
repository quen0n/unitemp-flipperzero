/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk
*/
#ifndef UNITEMP_MHZ19C_UART_H
#define UNITEMP_MHZ19C_UART_H

#include "../sensors.h"
#include <stdbool.h>

extern const SensorModel MHZ19C_UART;
extern const SensorConnectionInterface unitemp_mhz19c_uart;

/* Sound alert threshold (800..5000 ppm). */
uint16_t mhz19c_uart_get_alert(Sensor* sensor);
void mhz19c_uart_set_alert(Sensor* sensor, uint16_t alert_ppm);

/* Per-sensor LED / sound alert switches. */
bool mhz19c_uart_get_led(Sensor* sensor);
void mhz19c_uart_set_led(Sensor* sensor, bool enabled);
bool mhz19c_uart_get_sound(Sensor* sensor);
void mhz19c_uart_set_sound(Sensor* sensor, bool enabled);

#endif
