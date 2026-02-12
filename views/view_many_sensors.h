#pragma once

#include <gui/view.h>

typedef struct ManySensors ManySensors;

ManySensors* many_sensors_alloc(void* context);

void many_sensors_free(ManySensors* many_sensors);

View* many_sensors_get_view(ManySensors* many_sensors);

void many_sensors_refresh_data(ManySensors* instance);
