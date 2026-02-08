#pragma once

#include <gui/view.h>

typedef struct SingleSensor SingleSensor;

SingleSensor* single_sensor_alloc(void* context);

void single_sensor_free(SingleSensor* single_sensor);

View* single_sensor_get_view(SingleSensor* single_sensor);
