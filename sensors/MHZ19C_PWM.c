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
/* Smart spike/glitch rejection on the output value (all tunable):
   a jump bigger than (range_ppm / DIV) is held until it repeats CONFIRM cycles,
   so a single PWM mis-decode is ignored while a real, sustained change is followed.
   WARMUP_SAMPLES valid cycles must accumulate before the first value is shown
   (suppresses the cold-buffer reading right after the sensor screen opens). */
#define MHZ19C_DEGLITCH_DIV     32
#define MHZ19C_DEGLITCH_CONFIRM 2
#define MHZ19C_WARMUP_SAMPLES   3
/* Dedicated PWM-pin sampling period, ms. Decoupled from the sensor-list reader
   and the display — only the pulse timing needs this rate. */
#define MHZ19C_SAMPLE_PERIOD_MS 20

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
    /* Smart de-glitch filter state */
    int32_t co2_stable; /* last accepted value; <0 = not yet established */
    int32_t pend_val;   /* candidate of a pending (unconfirmed) jump */
    uint8_t pend_cnt;   /* consecutive cycles the pending jump has persisted */
    /* Dedicated fast sampler, independent of the sensor-list reader/redraw */
    FuriTimer* timer;
    /* Raw per-cycle ppm handoff: the fast timer writes raw_ppm and bumps raw_seq;
       the reader folds each new sequence into the average. The timer never writes
       sensor->co2 — the reader is the sole writer of it (no fast-writer race). */
    int32_t raw_ppm;
    uint32_t raw_seq;
    uint32_t raw_seq_seen;
    int32_t agg_co2; /* latest aggregated value (no offset); <0 = not ready */
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
    instance->co2_stable = -1;
    instance->agg_co2 = -1;
    instance->timer = NULL;
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
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance != NULL && instance->timer != NULL) {
        furi_timer_stop(instance->timer);
        furi_timer_free(instance->timer);
        instance->timer = NULL;
    }
    free(sensor->instance);
    sensor->instance = NULL;
    return true;
}

static void mhz19c_pwm_timer_cb(void* context);

static bool mhz19c_pwm_init(Sensor* sensor) {
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance == NULL) return false;

    instance->otg_was_enabled = furi_hal_power_is_otg_enabled();
    if(!instance->otg_was_enabled) {
        furi_hal_power_enable_otg();
    }

    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeInput, GpioPullUp, GpioSpeedVeryHigh);

    //Sample the PWM pin on a dedicated 20 ms timer, independent of the sensor-list
    //reader (250 ms) and the display — the ~1004 ms pulse must be timed finely.
    if(instance->timer == NULL) {
        instance->timer = furi_timer_alloc(mhz19c_pwm_timer_cb, FuriTimerTypePeriodic, sensor);
    }
    if(instance->timer != NULL) {
        furi_timer_start(instance->timer, furi_ms_to_ticks(MHZ19C_SAMPLE_PERIOD_MS));
    }
    return true;
}

static bool mhz19c_pwm_deinit(Sensor* sensor) {
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance != NULL && instance->timer != NULL) {
        furi_timer_stop(instance->timer); //stop sampling before releasing the pin
    }
    furi_hal_gpio_init(&gpio_ext_pa6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    if(instance != NULL && !instance->otg_was_enabled) {
        furi_hal_power_disable_otg();
    }
    return true;
}

/* Smart de-glitch: hold through single spikes, follow sustained changes.
   Returns the value to display, or -1 while still warming up. */
static int32_t mhz19c_pwm_deglitch(MHZ19CPwmInstance* in, int32_t candidate, bool ready) {
    if(in->co2_stable < 0) {
        //No stable value yet: wait for warmup, then seed from the first candidate
        if(!ready) return -1;
        in->co2_stable = candidate;
        in->pend_cnt = 0;
        return in->co2_stable;
    }

    int32_t band = in->range_ppm / MHZ19C_DEGLITCH_DIV;
    int32_t d = candidate - in->co2_stable;
    if(d < 0) d = -d;
    if(d <= band) {
        //Within the noise band: track it, clear any pending jump
        in->co2_stable = candidate;
        in->pend_cnt = 0;
        return in->co2_stable;
    }

    //Big jump: provisional. Accept only once it repeats CONFIRM cycles in the new region.
    int32_t pd = candidate - in->pend_val;
    if(pd < 0) pd = -pd;
    if(in->pend_cnt > 0 && pd <= band) {
        in->pend_cnt++;
    } else {
        in->pend_val = candidate;
        in->pend_cnt = 1;
    }
    if(in->pend_cnt >= MHZ19C_DEGLITCH_CONFIRM) {
        in->co2_stable = candidate;
        in->pend_cnt = 0;
    }
    //Otherwise keep showing the previous stable value (single spike suppressed)
    return in->co2_stable;
}

/* Fast PWM sampler: own 20 ms timer, independent of the sensor-list reader and
   the display. It ONLY times the pulse and publishes the raw per-cycle ppm to a
   private slot — no averaging, de-glitch or correction here, and it never writes
   sensor->co2. All the "cooking" and the single write of the on-screen value
   happen in the reader thread (mhz19c_pwm_update), like every other sensor, so
   nothing fast races the value the screen reads. */
static void mhz19c_pwm_timer_cb(void* context) {
    Sensor* sensor = context;
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance == NULL) return;

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

    //A full pulse decoded -> hand the raw ppm to the reader as one new sample.
    if(ppm > 0) {
        instance->raw_ppm = ppm;
        instance->raw_seq++;
    }
}

/* Reader thread (slow cadence): folds each freshly measured cycle into the
   average + de-glitch and is the SOLE writer of sensor->co2. Raw values arrive
   once per ~1 s pulse while the reader runs faster, so most calls just re-publish
   the current aggregate. The aggregate carries NO offset — the reader's generic
   post-step (sensors.c) adds the CO2 correction once on this fresh base, so the
   correction is never accumulated across calls. */
static SensorStatus mhz19c_pwm_update(Sensor* sensor) {
    MHZ19CPwmInstance* instance = sensor->instance;
    if(instance == NULL) return UT_SENSORSTATUS_ERROR;

    if(instance->needs_reset) {
        instance->needs_reset = false;
        instance->buf_idx = 0;
        instance->buf_count = 0;
        memset(instance->co2_buf, 0, sizeof(instance->co2_buf));
        instance->co2_stable = -1;
        instance->pend_val = 0;
        instance->pend_cnt = 0;
        instance->agg_co2 = -1;
        instance->raw_seq_seen = instance->raw_seq; //drop raw produced before reset
    }

    //Fold a freshly measured cycle (if any) into the average + de-glitch.
    if(instance->raw_seq != instance->raw_seq_seen) {
        instance->raw_seq_seen = instance->raw_seq;
        int32_t ppm = instance->raw_ppm;
        if(ppm > 0) {
            uint8_t win = instance->avg_win;
            if(win < 1) win = 1;
            if(win > MHZ19C_CO2_BUF_MAX) win = MHZ19C_CO2_BUF_MAX;

            int32_t candidate;
            bool ready;
            if(win == 1) {
                //Raw mode: no averaging, but the de-glitch filter still suppresses spikes
                candidate = ppm;
                ready = true;
            } else {
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

                //Trimmed mean (drop min+max), kept identical to air_stats
                if(instance->buf_count < 3) {
                    candidate = sorted[instance->buf_count / 2];
                } else {
                    int32_t sum = 0;
                    for(uint8_t i = 1; i < instance->buf_count - 1; i++) sum += sorted[i];
                    candidate = sum / (int32_t)(instance->buf_count - 2);
                }
                ready = (instance->buf_count >= MHZ19C_WARMUP_SAMPLES);
            }

            //Smart spike/glitch rejection on top of the average
            int32_t out = mhz19c_pwm_deglitch(instance, candidate, ready);
            if(out >= 0) instance->agg_co2 = out;
        }
    }

    //Re-publish the aggregate every call so the screen and the offset step always
    //read a complete, freshly written value (no cross-call accumulation).
    if(instance->agg_co2 < 0) {
        sensor->co2 = -1.0f;
        return UT_SENSORSTATUS_POLLING;
    }
    sensor->co2 = (float)instance->agg_co2;
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
