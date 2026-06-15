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
#include "../sensors/MHZ19C_PWM.h"
#include <locale/locale.h>

static EnvironmentState last_enviroment_state = EnvironmentStateUndefined;
/* Unified "what the lamp currently shows" dedup cache (see unitemp_apply_led):
   the single source of truth for BOTH the heat-index and the CO2 indication, so
   the two can never desync and the lamp on any screen is a pure function of that
   screen's content — not of how you navigated to it. -1 = unknown. */
static int16_t led_shown = -1;

static const NotificationMessage message_green_128 = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 128,
};
// static const NotificationMessage message_green_64 = {
//     .type = NotificationMessageTypeLedGreen,
//     .data.led.value = 64,
// };
// static const NotificationMessage message_red_128 = {
//     .type = NotificationMessageTypeLedRed,
//     .data.led.value = 128,
// };
static const NotificationMessage message_red_191 = {
    .type = NotificationMessageTypeLedRed,
    .data.led.value = 191,
};

const NotificationMessage message_blink_start_125 = {
    .type = NotificationMessageTypeLedBlinkStart,
    .data.led_blink.color = 0,
    .data.led_blink.on_time = 75,
    .data.led_blink.period = 150,
};

static const NotificationSequence EnvironmentStateUndefinedSequence = {
    &message_blink_stop,
    &message_red_0,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    &message_green_0,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence EnvironmentStateExcellentSequence = {
    &message_blink_stop,
    &message_red_0,
    &message_do_not_reset,
    &message_green_255,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence EnvironmentStateGoodSequence = {
    &message_blink_stop,
    &message_red_191,
    &message_do_not_reset,
    &message_green_255,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence EnvironmentStateModerateSequence = {
    &message_blink_stop,
    &message_red_255,
    &message_do_not_reset,
    &message_green_128,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence EnvironmentStatePoorSequence = {
    &message_blink_stop,
    &message_red_255,
    &message_do_not_reset,
    &message_green_0,
    &message_do_not_reset,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence EnvironmentStateDangerousSequence = {
    &message_blink_start_125,
    &message_do_not_reset,
    &message_blink_set_color_red,
    &message_do_not_reset,
    NULL};

static const NotificationSequence* notification_sequences[6] = {
    &EnvironmentStateUndefinedSequence,
    &EnvironmentStateExcellentSequence,
    &EnvironmentStateGoodSequence,
    &EnvironmentStateModerateSequence,
    &EnvironmentStatePoorSequence,
    &EnvironmentStateDangerousSequence};

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
       temperature_in_fahrenheit < 70.0f || temperature_in_fahrenheit > 115.0f) {
        return temperature_in_fahrenheit;
    }

    double temp_f = (double)temperature_in_fahrenheit;
    double rh = (double)humidity_in_percent;
    const double c1 = 0.363445176;
    const double c2 = 0.988622465;
    const double c3 = 4.777114035;
    const double c4 = -0.114037667;
    const double c5 = -8.50208e-4;
    const double c6 = -2.0716198e-2;
    const double c7 = 6.87678e-4;
    const double c8 = 2.74954e-4;
    const double c9 = 0.0;

    double hi_f = c1 + c2 * temp_f + c3 * rh + c4 * temp_f * rh + c5 * temp_f * temp_f +
                  c6 * rh * rh + c7 * temp_f * temp_f * rh + c8 * temp_f * rh * rh +
                  c9 * temp_f * temp_f * rh * rh;

    return (float)hi_f;
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

//https://en.wikipedia.org/wiki/Heat_index#Effects_of_the_heat_index_(shade_values)
EnvironmentState unitemp_determine_environment_state_from_hi(float heat_index_in_fahrenheit) {
    if(heat_index_in_fahrenheit < 80.0f) {
        return EnvironmentStateExcellent;
    } else if(heat_index_in_fahrenheit > 80.0f && heat_index_in_fahrenheit <= 90.0f) {
        return EnvironmentStateGood;
    } else if(heat_index_in_fahrenheit > 90.0f && heat_index_in_fahrenheit <= 106.0f) {
        return EnvironmentStateModerate;
    } else if(heat_index_in_fahrenheit > 106.0f && heat_index_in_fahrenheit <= 129.0f) {
        return EnvironmentStatePoor;
    } else if(heat_index_in_fahrenheit > 129.0f) {
        return EnvironmentStateDangerous;
    } else
        return EnvironmentStateUndefined;
}

EnvironmentState unitemp_determine_environment_state_from_co2(uint16_t ppm) {
    if(ppm <= 400.0f) {
        return EnvironmentStateExcellent;
    } else if(ppm > 400.0f && ppm <= 1000.0f) {
        return EnvironmentStateGood;
    } else if(ppm > 1000.0f && ppm <= 5000.0f) {
        return EnvironmentStateModerate;
    } else if(ppm > 5000.0f && ppm <= 15000.0f) {
        return EnvironmentStatePoor;
    } else if(ppm > 15000.0f) {
        return EnvironmentStateDangerous;
    } else
        return EnvironmentStateUndefined;
}

EnvironmentState unitemp_determine_environment_state(Sensor* sensor) {
    //CO2-only sensors are handled by unitemp_co2_alerts_tick (steady LED levels,
    //one-shot sound), not by the environment state machinery
    if(sensor->model->data_type == UT_DATA_TYPE_CO2) {
        return EnvironmentStateUndefined;
    }
    if(sensor->status != UT_SENSORSTATUS_OK) return EnvironmentStateUndefined;
    //Combo sensor (climate + CO2): if it has CO2, the lamp follows CO2 ONLY,
    //not the heat-index. CO2 takes over the indication whenever it is present.
    if(sensor->model->data_type == UT_DATA_TYPE_TEMP_HUM_CO2) {
        return unitemp_determine_environment_state_from_co2(sensor->co2);
    }
    //Climate-only sensors: heat-index from temperature + humidity
    EnvironmentState hi_state = EnvironmentStateUndefined;
    if(sensor->model->data_type == UT_DATA_TYPE_TEMP_HUM ||
       sensor->model->data_type == UT_DATA_TYPE_TEMP_HUM_PRESS) {
        hi_state = unitemp_determine_environment_state_from_hi(unitemp_calculate_heat_index(
            locale_celsius_to_fahrenheit(sensor->temperature), sensor->humidity));
    }
    return hi_state;
}

void unitemp_display_environment_state(
    NotificationApp* app,
    EnvironmentState state,
    bool light,
    bool vibro_and_sound) {
    if(state == last_enviroment_state) {
        // if(state == EnvironmentStateDangerous && vibro_and_sound) {
        //     notification_message(app, &sequence_audiovisual_alert);
        // }
        return;
    }
    last_enviroment_state = state;

    UNITEMP_DEBUG("Environment state %d", state);

    if(light) {
        notification_message(app, notification_sequences[state]);
    }
    if(state == EnvironmentStateDangerous && vibro_and_sound) {
        notification_message(app, &sequence_audiovisual_alert);
    }
}
void unitemp_reset_environment_state(NotificationApp* app) {
    last_enviroment_state = EnvironmentStateUndefined;
    led_shown = 0; //LED is now physically off — keep the unified cache in sync

    notification_message(app, &sequence_blink_stop);
    notification_message(app, &sequence_reset_rgb);
}

/* ---- CO2 LED & sound alerts (logic ported from flipper-air-stats) ------ */

//LED level boundaries, ppm
#define UNITEMP_CO2_LED_YELLOW_PPM 800
#define UNITEMP_CO2_LED_ORANGE_PPM 1000
#define UNITEMP_CO2_LED_RED_PPM    1400
//Sound alert hysteresis and cooldown
#define UNITEMP_CO2_HYST_PPM    50
#define UNITEMP_CO2_COOLDOWN_MS 60000UL

/* Steady LED colors (do_not_reset = persists through other notifications).
   Every sequence sets ALL three channels so no color leaks from the previous one. */
static const NotificationSequence co2_seq_led_green = {
    &message_red_0,
    &message_green_255,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence co2_seq_led_yellow = {
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationMessage co2_led_green_80 = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 80,
};
static const NotificationSequence co2_seq_led_orange = {
    &message_red_255,
    &co2_led_green_80,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence co2_seq_led_red = {
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};
static const NotificationSequence co2_seq_led_off = {
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};

static const NotificationMessage co2_vol_msg = {
    .type = NotificationMessageTypeForceSpeakerVolumeSetting,
    .data.forced_settings.speaker_volume = 0.5f,
};
static const NotificationMessage co2_note_low = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 880.0f, .volume = 1.0f},
};
static const NotificationMessage co2_note_high = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 1174.7f, .volume = 1.0f},
};
static const NotificationMessage co2_delay_120 = {
    .type = NotificationMessageTypeDelay,
    .data.delay.length = 120,
};
static const NotificationMessage co2_sound_off = {
    .type = NotificationMessageTypeSoundOff,
};
/* Alarm (OK→BAD): two rising notes */
static const NotificationSequence co2_seq_alarm = {
    &co2_vol_msg,
    &co2_note_low,
    &co2_delay_120,
    &co2_note_high,
    &co2_delay_120,
    &co2_sound_off,
    &message_do_not_reset,
    NULL,
};
/* Relief (BAD→OK): two falling notes */
static const NotificationSequence co2_seq_relief = {
    &co2_vol_msg,
    &co2_note_high,
    &co2_delay_120,
    &co2_note_low,
    &co2_delay_120,
    &co2_sound_off,
    &message_do_not_reset,
    NULL,
};

static bool co2_was_above = false;
static uint32_t co2_last_alert_tick = 0;

/* Apply an LED code idempotently: touch the hardware ONLY when the code actually
   changes. This dedup is the only retained state — the decision itself is fully
   recomputed from the current screen every tick, so the lamp is path-independent.
   Codes: 0..5 = heat-index EnvironmentState (0 = Undefined = off); 0x11..0x14 =
   CO2 level green/yellow/orange/red. */
static void unitemp_apply_led(NotificationApp* notifications, int16_t code) {
    if(code == led_shown) return;
    led_shown = code;
    if(code & 0x10) {
        const NotificationSequence* seq;
        switch(code & 0x0F) {
        case 1:
            seq = &co2_seq_led_green;
            break;
        case 2:
            seq = &co2_seq_led_yellow;
            break;
        case 3:
            seq = &co2_seq_led_orange;
            break;
        case 4:
            seq = &co2_seq_led_red;
            break;
        default:
            seq = &co2_seq_led_off;
            break;
        }
        notification_message(notifications, seq);
    } else {
        notification_message(notifications, notification_sequences[code]);
    }
}

void unitemp_co2_alerts_reset(void) {
    led_shown = -1; //force the lamp to repaint from the current screen next tick
}

void unitemp_co2_alerts_stop(void* context) {
    UnitempApp* app = context;
    unitemp_reset_environment_state(app->notifications);
}

/* One-shot CO2 sound on threshold crossings (hysteresis + cooldown). Stateful by
   nature (edge detection on the CO2 value), separate from the LED dedup cache. */
static void unitemp_co2_sound_tick(UnitempApp* app, Sensor* sensor, bool has_data) {
    if(!app->settings->environment_state_sound_and_vibro_indication) return;
    if(!mhz19c_pwm_get_sound(sensor) || !has_data) return;

    uint16_t alert = mhz19c_pwm_get_alert(sensor);
    bool above = sensor->co2 >= (float)alert;
    bool way_below = sensor->co2 < (float)(alert - UNITEMP_CO2_HYST_PPM);
    bool cooldown_expired =
        (furi_get_tick() - co2_last_alert_tick) >= furi_ms_to_ticks(UNITEMP_CO2_COOLDOWN_MS);

    if(above && !co2_was_above) {
        if(cooldown_expired) {
            notification_message(app->notifications, &co2_seq_alarm);
            co2_last_alert_tick = furi_get_tick();
        }
        co2_was_above = true;
    }
    if(way_below && co2_was_above) {
        if(cooldown_expired) {
            notification_message(app->notifications, &co2_seq_relief);
            co2_last_alert_tick = furi_get_tick();
        }
        co2_was_above = false;
    }
}

void unitemp_co2_alerts_tick(void* context, Sensor* sensor) {
    UnitempApp* app = context;
    //sensor is the CO2 source shown on the active screen. The mhz19c_pwm_*
    //getters self-guard (model != MHZ19C_PWM -> defaults), so a combo/UART
    //source safely runs the LED/sound on default settings.
    if(sensor == NULL) {
        unitemp_apply_led(app->notifications, 0);
        return;
    }

    bool has_data = (sensor->status == UT_SENSORSTATUS_OK ||
                     sensor->status == UT_SENSORSTATUS_POLLING) &&
                    sensor->co2 > 0.0f && !mhz19c_pwm_is_frozen(sensor);

    //LED: steady colour by level; off when no data or disabled
    int16_t code = 0;
    if(app->settings->environment_state_led_indication && mhz19c_pwm_get_led(sensor) &&
       has_data) {
        if(sensor->co2 < (float)UNITEMP_CO2_LED_YELLOW_PPM) {
            code = 0x11;
        } else if(sensor->co2 < (float)UNITEMP_CO2_LED_ORANGE_PPM) {
            code = 0x12;
        } else if(sensor->co2 < (float)UNITEMP_CO2_LED_RED_PPM) {
            code = 0x13;
        } else {
            code = 0x14;
        }
    }
    unitemp_apply_led(app->notifications, code);

    unitemp_co2_sound_tick(app, sensor, has_data);
}

void unitemp_indication_tick(void* context, Sensor* co2_sensor, Sensor* climate_sensor) {
    UnitempApp* app = context;
    NotificationApp* notifications = app->notifications;

    //The lamp is a pure function of the CURRENT screen, recomputed every tick and
    //applied through ONE unified cache (unitemp_apply_led). No mode-reset and no
    //path-dependent second cache, so the same screen always yields the same lamp,
    //regardless of which screen you came from.

    //CO2 on screen -> CO2 owns the lamp (colour by ppm, off when it has no data)
    if(co2_sensor != NULL) {
        //CO2 path: arm the heat-index danger-edge so a later climate Dangerous re-alerts
        last_enviroment_state = EnvironmentStateUndefined;
        unitemp_co2_alerts_tick(app, co2_sensor);
        return;
    }

    //No CO2 on screen -> stock heat-index of the shown climate sensor (off when
    //there is no climate sensor, or it has no heat-index: no humidity / no data)
    EnvironmentState state = climate_sensor != NULL ?
                                 unitemp_determine_environment_state(climate_sensor) :
                                 EnvironmentStateUndefined;
    bool dangerous = (state == EnvironmentStateDangerous);
    bool entering_danger = dangerous && (last_enviroment_state != EnvironmentStateDangerous);
    last_enviroment_state = state; //tracked only for the Dangerous one-shot edge

    int16_t code = app->settings->environment_state_led_indication ? (int16_t)state : 0;

    if(dangerous && app->settings->infinity_backlight) {
        notification_message(notifications, &sequence_display_backlight_enforce_auto);
    }
    unitemp_apply_led(notifications, code);
    if(dangerous && app->settings->infinity_backlight) {
        notification_message(notifications, &sequence_display_backlight_enforce_on);
    }
    if(entering_danger) {
        notification_message(notifications, &sequence_audiovisual_alert);
    }
}
