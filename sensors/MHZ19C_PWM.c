#include "MHZ19C_PWM.h"

#include "../helpers/unitemp_gpio.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_power.h>
#include <stdlib.h>
#include <string.h>

/* The PWM input is fixed to pin 3 (A6) of the Flipper GPIO header. */
#define MHZ19C_PWM_PIN_NUM 3

#define MHZ19C_CO2_BUF_MAX       30
#define MHZ19C_RANGE_DEFAULT_PPM 5000
#define MHZ19C_AVG_DEFAULT       5
#define MHZ19C_ALERT_DEFAULT_PPM 1000
#define MHZ19C_FREEZE_TIMEOUT_MS 5000

typedef struct {
    int32_t prev_value;
    int32_t high_ms;
    int32_t low_ms;
    int32_t high_tick;
    int32_t low_tick;
    int32_t co2_buf[MHZ19C_CO2_BUF_MAX];
    uint8_t buf_idx;
    uint8_t buf_count;
    bool otg_was_enabled;
    uint32_t last_edge_tick;
    bool needs_reset;
    /* PWM averaging window, 1..30 */
    uint8_t avg_win;
    /* Sensor detection range (PWM scale), ppm */
    uint16_t range_ppm;
    /* Sound alert threshold, ppm */
    uint16_t alert_ppm;
    /* LED indication of the CO2 level */
    bool led_enabled;
    /* Sound alert on crossing the threshold */
    bool sound_enabled;
} MHZ19CPwmInstance;

static int32_t mhz19c_pwm_calculate_ppm(
    int32_t* prev_value,
    int32_t value,
    int32_t* high_ms,
    int32_t* low_ms,
    int32_t* high_tick,
    int32_t* low_tick,
    int32_t range_ppm) {
    int32_t now = furi_get_tick();
    if(value == 1) {
        if(value != *prev_value) {
            *high_tick = now;
            *low_ms = *high_tick - *low_tick;
            *prev_value = value;
        }
    } else if(value != *prev_value) {
        *low_tick = now;
        *high_ms = *low_tick - *high_tick;
        *prev_value = value;
        int32_t period = *high_ms + *low_ms - 4;
        if(period <= 0) return -1;
        return range_ppm * (*high_ms - 2) / period;
    }
    return -1;
}

static bool mhz19c_pwm_alloc(Sensor* sensor, char* args) {
    MHZ19CPwmInstance* instance = malloc(sizeof(MHZ19CPwmInstance));
    if(instance == NULL) return false;
    memset(instance, 0, sizeof(MHZ19CPwmInstance));

    instance->avg_win = MHZ19C_AVG_DEFAULT;
    instance->range_ppm = MHZ19C_RANGE_DEFAULT_PPM;
    instance->alert_ppm = MHZ19C_ALERT_DEFAULT_PPM;
    instance->led_enabled = true;
    instance->sound_enabled = true;
    //Optional args: "<avg 1..30> <range 2000..10000> [<alert 800..5000> <led 0/1> <sound 0/1>]"
    if(args != NULL) {
        int avg = MHZ19C_AVG_DEFAULT;
        int range = MHZ19C_RANGE_DEFAULT_PPM;
        int alert = MHZ19C_ALERT_DEFAULT_PPM;
        int led = 1;
        int sound = 1;
        int parsed = sscanf(args, "%d %d %d %d %d", &avg, &range, &alert, &led, &sound);
        if(parsed >= 2) {
            if(avg >= 1 && avg <= MHZ19C_CO2_BUF_MAX) instance->avg_win = (uint8_t)avg;
            if(range >= 2000 && range <= 10000) instance->range_ppm = (uint16_t)range;
        }
        if(parsed >= 5) {
            if(alert >= 800 && alert <= 5000) instance->alert_ppm = (uint16_t)alert;
            instance->led_enabled = (led != 0);
            instance->sound_enabled = (sound != 0);
        }
    }

    sensor->instance = instance;
    sensor->co2 = -1.0f;
    return true;
}

static bool mhz19c_pwm_free(Sensor* sensor) {
    free(sensor->instance);
    sensor->instance = NULL;
    return true;
}

static bool mhz19c_pwm_init(Sensor* sensor) {
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance == NULL) return false;

    instance->otg_was_enabled = furi_hal_power_is_otg_enabled();
    if(!instance->otg_was_enabled) {
        furi_hal_power_enable_otg();
    }

    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeInput, GpioPullUp, GpioSpeedVeryHigh);
    return true;
}

static bool mhz19c_pwm_deinit(Sensor* sensor) {
    MHZ19CPwmInstance* instance = sensor->instance;
    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    if(instance != NULL && !instance->otg_was_enabled) {
        furi_hal_power_disable_otg();
    }
    return true;
}

static SensorStatus mhz19c_pwm_update(Sensor* sensor) {
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance == NULL) return UT_SENSORSTATUS_ERROR;

    if(instance->needs_reset) {
        instance->needs_reset = false;
        sensor->co2 = -1.0f;
        instance->buf_idx = 0;
        instance->buf_count = 0;
        instance->prev_value = 0;
        instance->high_ms = 0;
        instance->low_ms = 0;
        instance->high_tick = 0;
        instance->low_tick = 0;
        instance->last_edge_tick = 0;
        memset(instance->co2_buf, 0, sizeof(instance->co2_buf));
    }

    int32_t old_prev = instance->prev_value;
    int32_t ppm = mhz19c_pwm_calculate_ppm(
        &instance->prev_value,
        furi_hal_gpio_read(&gpio_ext_pa6) ? 1 : 0,
        &instance->high_ms,
        &instance->low_ms,
        &instance->high_tick,
        &instance->low_tick,
        instance->range_ppm);

    if(instance->prev_value != old_prev) {
        instance->last_edge_tick = furi_get_tick();
    }

    if(ppm <= 0) return UT_SENSORSTATUS_POLLING;

    uint8_t win = instance->avg_win;
    if(win < 1) win = 1;
    if(win > MHZ19C_CO2_BUF_MAX) win = MHZ19C_CO2_BUF_MAX;

    if(win == 1) {
        sensor->co2 = (float)ppm;
        return UT_SENSORSTATUS_OK;
    }

    instance->co2_buf[instance->buf_idx] = ppm;
    instance->buf_idx = (instance->buf_idx + 1) % win;
    if(instance->buf_count < win) instance->buf_count++;

    int32_t sorted[MHZ19C_CO2_BUF_MAX];
    memcpy(sorted, instance->co2_buf, instance->buf_count * sizeof(int32_t));
    for(uint8_t i = 1; i < instance->buf_count; i++) {
        int32_t key = sorted[i];
        int8_t j = (int8_t)i - 1;
        while(j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    int32_t filtered = 0;
    if(instance->buf_count < 3) {
        filtered = sorted[instance->buf_count / 2];
    } else {
        int32_t sum = 0;
        for(uint8_t i = 1; i < instance->buf_count - 1; i++) sum += sorted[i];
        filtered = sum / (int32_t)(instance->buf_count - 2);
    }

    sensor->co2 = (float)filtered;
    return UT_SENSORSTATUS_OK;
}

static bool mhz19c_pwm_if_alloc(Sensor* sensor, char* args) {
    if(!sensor->model->allocator(sensor, args)) return false;
    unitemp_gpio_lock(unitemp_gpio_get_from_int(MHZ19C_PWM_PIN_NUM), &unitemp_mhz19c_pwm);
    return true;
}

static bool mhz19c_pwm_if_free(Sensor* sensor) {
    bool result = sensor->model->mem_releaser(sensor);
    unitemp_gpio_unlock(unitemp_gpio_get_from_int(MHZ19C_PWM_PIN_NUM));
    return result;
}

static SensorStatus mhz19c_pwm_if_update(Sensor* sensor) {
    return sensor->model->updater(sensor);
}

const SensorConnectionInterface unitemp_mhz19c_pwm = {
    .name = "DirectGPIO",
    .allocator = mhz19c_pwm_if_alloc,
    .mem_releaser = mhz19c_pwm_if_free,
    .updater = mhz19c_pwm_if_update,
};

bool mhz19c_pwm_is_frozen(Sensor* sensor) {
    if(sensor == NULL || sensor->model != &MHZ19C_PWM || sensor->instance == NULL) {
        return false;
    }
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance->last_edge_tick == 0) return false;
    return (furi_get_tick() - instance->last_edge_tick) > MHZ19C_FREEZE_TIMEOUT_MS;
}

void mhz19c_pwm_request_reset(Sensor* sensor) {
    if(sensor == NULL || sensor->model != &MHZ19C_PWM || sensor->instance == NULL) {
        return;
    }
    MHZ19CPwmInstance* instance = sensor->instance;
    instance->needs_reset = true;
}

static bool mhz19c_pwm_instance_valid(Sensor* sensor) {
    return sensor != NULL && sensor->model == &MHZ19C_PWM && sensor->instance != NULL;
}

uint8_t mhz19c_pwm_get_avg(Sensor* sensor) {
    if(!mhz19c_pwm_instance_valid(sensor)) return MHZ19C_AVG_DEFAULT;
    return ((MHZ19CPwmInstance*)sensor->instance)->avg_win;
}

void mhz19c_pwm_set_avg(Sensor* sensor, uint8_t avg) {
    if(!mhz19c_pwm_instance_valid(sensor)) return;
    if(avg < 1 || avg > MHZ19C_CO2_BUF_MAX) return;
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance->avg_win != avg) {
        instance->avg_win = avg;
        instance->needs_reset = true; //window changed — start averaging over
    }
}

uint16_t mhz19c_pwm_get_range(Sensor* sensor) {
    if(!mhz19c_pwm_instance_valid(sensor)) return MHZ19C_RANGE_DEFAULT_PPM;
    return ((MHZ19CPwmInstance*)sensor->instance)->range_ppm;
}

void mhz19c_pwm_set_range(Sensor* sensor, uint16_t range_ppm) {
    if(!mhz19c_pwm_instance_valid(sensor)) return;
    if(range_ppm < 2000 || range_ppm > 10000) return;
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance->range_ppm != range_ppm) {
        instance->range_ppm = range_ppm;
        instance->needs_reset = true; //scale changed — old samples are wrong
    }
}

uint16_t mhz19c_pwm_get_alert(Sensor* sensor) {
    if(!mhz19c_pwm_instance_valid(sensor)) return MHZ19C_ALERT_DEFAULT_PPM;
    return ((MHZ19CPwmInstance*)sensor->instance)->alert_ppm;
}

void mhz19c_pwm_set_alert(Sensor* sensor, uint16_t alert_ppm) {
    if(!mhz19c_pwm_instance_valid(sensor)) return;
    if(alert_ppm < 800 || alert_ppm > 5000) return;
    ((MHZ19CPwmInstance*)sensor->instance)->alert_ppm = alert_ppm;
}

bool mhz19c_pwm_get_led(Sensor* sensor) {
    if(!mhz19c_pwm_instance_valid(sensor)) return true;
    return ((MHZ19CPwmInstance*)sensor->instance)->led_enabled;
}

void mhz19c_pwm_set_led(Sensor* sensor, bool enabled) {
    if(!mhz19c_pwm_instance_valid(sensor)) return;
    ((MHZ19CPwmInstance*)sensor->instance)->led_enabled = enabled;
}

bool mhz19c_pwm_get_sound(Sensor* sensor) {
    if(!mhz19c_pwm_instance_valid(sensor)) return true;
    return ((MHZ19CPwmInstance*)sensor->instance)->sound_enabled;
}

void mhz19c_pwm_set_sound(Sensor* sensor, bool enabled) {
    if(!mhz19c_pwm_instance_valid(sensor)) return;
    ((MHZ19CPwmInstance*)sensor->instance)->sound_enabled = enabled;
}

const SensorModel MHZ19C_PWM = {
    .modelname = "MHZ19C",
    .altname = "MH-Z19C (PWM)",
    .data_type = UT_DATA_TYPE_CO2,
    .interface = &unitemp_mhz19c_pwm,
    .polling_interval = 20,
    .allocator = mhz19c_pwm_alloc,
    .mem_releaser = mhz19c_pwm_free,
    .initializer = mhz19c_pwm_init,
    .deinitializer = mhz19c_pwm_deinit,
    .updater = mhz19c_pwm_update,
};
