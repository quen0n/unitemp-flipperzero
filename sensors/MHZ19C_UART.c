/*
    Unitemp - Universal temperature reader
    MH-Z19C CO2 sensor over UART (LPUART1, 9600 baud).
    Ported from flipper-air-stats. Pin 15 (C1, TX) -> sensor RX, pin 16 (C0, RX) <- sensor TX.
    Protocol: 9-byte request/response, checksum = 0xFF - sum(bytes[1..7]) + 1.
*/
#include "MHZ19C_UART.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_power.h>
#include <stdlib.h>
#include <string.h>

#define MHZ19C_UART_BUF_SIZE          9u
#define MHZ19C_UART_CMD_GAS_CONC      0x86
#define MHZ19C_UART_ALERT_DEFAULT_PPM 1000

typedef struct {
    FuriStreamBuffer* stream;
    FuriHalSerialHandle* serial;
    bool otg_was_enabled;
    /* Sound alert threshold, ppm */
    uint16_t alert_ppm;
    /* Per-sensor LED / sound switches */
    bool led_enabled;
    bool sound_enabled;
} MHZ19CUartInstance;

static uint8_t mhz19c_uart_checksum(uint8_t* pkt) {
    uint8_t cs = 0;
    for(uint8_t i = 1; i < 8; i++) cs += pkt[i];
    return (uint8_t)(0xFF - cs + 1);
}

static void
    mhz19c_uart_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    FuriStreamBuffer* stream = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(stream, &data, 1, 0);
    }
}

static bool mhz19c_uart_alloc(Sensor* sensor, char* args) {
    MHZ19CUartInstance* instance = malloc(sizeof(MHZ19CUartInstance));
    if(instance == NULL) return false;
    memset(instance, 0, sizeof(MHZ19CUartInstance));

    instance->alert_ppm = MHZ19C_UART_ALERT_DEFAULT_PPM;
    instance->led_enabled = true;
    instance->sound_enabled = true;
    //Optional args: "<alert 800..5000> <led 0/1> <sound 0/1>"
    if(args != NULL) {
        int alert = MHZ19C_UART_ALERT_DEFAULT_PPM;
        int led = 1;
        int sound = 1;
        int parsed = sscanf(args, "%d %d %d", &alert, &led, &sound);
        if(parsed >= 1 && alert >= 800 && alert <= 5000) instance->alert_ppm = (uint16_t)alert;
        if(parsed >= 3) {
            instance->led_enabled = (led != 0);
            instance->sound_enabled = (sound != 0);
        }
    }

    instance->stream = furi_stream_buffer_alloc(32, MHZ19C_UART_BUF_SIZE);
    if(instance->stream == NULL) {
        free(instance);
        return false;
    }
    instance->serial = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    if(instance->serial == NULL) {
        furi_stream_buffer_free(instance->stream);
        free(instance);
        return false;
    }

    sensor->instance = instance;
    sensor->co2 = -1.0f;
    return true;
}

static bool mhz19c_uart_free(Sensor* sensor) {
    MHZ19CUartInstance* instance = sensor->instance;
    if(instance != NULL) {
        if(instance->serial != NULL) furi_hal_serial_control_release(instance->serial);
        if(instance->stream != NULL) furi_stream_buffer_free(instance->stream);
        free(instance);
        sensor->instance = NULL;
    }
    return true;
}

static bool mhz19c_uart_init(Sensor* sensor) {
    MHZ19CUartInstance* instance = sensor->instance;
    if(instance == NULL || instance->serial == NULL) return false;

    instance->otg_was_enabled = furi_hal_power_is_otg_enabled();
    if(!instance->otg_was_enabled) {
        furi_hal_power_enable_otg();
    }

    furi_hal_serial_init(instance->serial, 9600);
    furi_hal_serial_async_rx_start(instance->serial, mhz19c_uart_rx_cb, instance->stream, false);
    return true;
}

static bool mhz19c_uart_deinit(Sensor* sensor) {
    MHZ19CUartInstance* instance = sensor->instance;
    if(instance != NULL && instance->serial != NULL) {
        furi_hal_serial_async_rx_stop(instance->serial);
        furi_hal_serial_deinit(instance->serial);
    }
    if(instance != NULL && !instance->otg_was_enabled) {
        furi_hal_power_disable_otg();
    }
    return true;
}

static SensorStatus mhz19c_uart_update(Sensor* sensor) {
    MHZ19CUartInstance* instance = sensor->instance;
    if(instance == NULL) return UT_SENSORSTATUS_ERROR;

    uint8_t buf[MHZ19C_UART_BUF_SIZE] = {0};
    furi_stream_buffer_reset(instance->stream);

    buf[0] = 0xFF;
    buf[1] = 0x01;
    buf[2] = MHZ19C_UART_CMD_GAS_CONC;
    buf[8] = mhz19c_uart_checksum(buf);
    furi_hal_serial_tx(instance->serial, buf, sizeof(buf));

    size_t read = furi_stream_buffer_receive(instance->stream, buf, sizeof(buf), 50);
    if(read != MHZ19C_UART_BUF_SIZE) {
        sensor->co2 = -1.0f;
        return UT_SENSORSTATUS_TIMEOUT;
    }
    //Bad checksum -> treat as a failed poll (no dedicated BADCRC status in this fork)
    if(buf[8] != mhz19c_uart_checksum(buf)) {
        sensor->co2 = -1.0f;
        return UT_SENSORSTATUS_TIMEOUT;
    }

    sensor->co2 = (float)((uint32_t)buf[2] * 256 + buf[3]);
    return UT_SENSORSTATUS_OK;
}

/* ---- Connection interface (serial, no GPIO lock) ---- */

static bool mhz19c_uart_if_alloc(Sensor* sensor, char* args) {
    return sensor->model->allocator(sensor, args);
}

static bool mhz19c_uart_if_free(Sensor* sensor) {
    return sensor->model->mem_releaser(sensor);
}

static SensorStatus mhz19c_uart_if_update(Sensor* sensor) {
    return sensor->model->updater(sensor);
}

const SensorConnectionInterface unitemp_mhz19c_uart = {
    .name = "DirectUART",
    .allocator = mhz19c_uart_if_alloc,
    .mem_releaser = mhz19c_uart_if_free,
    .updater = mhz19c_uart_if_update,
};

/* ---- Per-sensor settings accessors ---- */

static bool mhz19c_uart_instance_valid(Sensor* sensor) {
    return sensor != NULL && sensor->model == &MHZ19C_UART && sensor->instance != NULL;
}

uint16_t mhz19c_uart_get_alert(Sensor* sensor) {
    if(!mhz19c_uart_instance_valid(sensor)) return MHZ19C_UART_ALERT_DEFAULT_PPM;
    return ((MHZ19CUartInstance*)sensor->instance)->alert_ppm;
}

void mhz19c_uart_set_alert(Sensor* sensor, uint16_t alert_ppm) {
    if(!mhz19c_uart_instance_valid(sensor)) return;
    if(alert_ppm < 800 || alert_ppm > 5000) return;
    ((MHZ19CUartInstance*)sensor->instance)->alert_ppm = alert_ppm;
}

bool mhz19c_uart_get_led(Sensor* sensor) {
    if(!mhz19c_uart_instance_valid(sensor)) return true;
    return ((MHZ19CUartInstance*)sensor->instance)->led_enabled;
}

void mhz19c_uart_set_led(Sensor* sensor, bool enabled) {
    if(!mhz19c_uart_instance_valid(sensor)) return;
    ((MHZ19CUartInstance*)sensor->instance)->led_enabled = enabled;
}

bool mhz19c_uart_get_sound(Sensor* sensor) {
    if(!mhz19c_uart_instance_valid(sensor)) return true;
    return ((MHZ19CUartInstance*)sensor->instance)->sound_enabled;
}

void mhz19c_uart_set_sound(Sensor* sensor, bool enabled) {
    if(!mhz19c_uart_instance_valid(sensor)) return;
    ((MHZ19CUartInstance*)sensor->instance)->sound_enabled = enabled;
}

const SensorModel MHZ19C_UART = {
    //Modelname doubles as the default sensor name and the save-file key. Mirrors
    //unitemp's two-variant convention (cf. AM2320 / AM2320_I2C); the PWM sibling owns
    //the bare chip name "MHZ19C", so this one adds the interface initial "U". Kept to
    //7 chars so it renders whole on every screen — the grid tile clips names past
    //~56 px (FontPrimary) and the mix pair-header shares 116 px. The full interface
    //label lives in altname "MH-Z19C (UART)" (shown in the add list).
    .modelname = "MHZ19CU",
    .altname = "MH-Z19C (UART)",
    .data_type = UT_DATA_TYPE_CO2,
    .interface = &unitemp_mhz19c_uart,
    .polling_interval = 5000,
    .allocator = mhz19c_uart_alloc,
    .mem_releaser = mhz19c_uart_free,
    .initializer = mhz19c_uart_init,
    .deinitializer = mhz19c_uart_deinit,
    .updater = mhz19c_uart_update,
};
