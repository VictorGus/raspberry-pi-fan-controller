#include "raspberry.h"

static int sim_fan_on = 0;
static int sim_temp = 35;

int get_fan_pin() {
  return 4;
}

int turn_on_fan(int pin) {
  (void)pin;
  sim_fan_on = 1;
  return FAN_ON_SUCCESS;
}

int turn_off_fan(int pin) {
  (void)pin;
  sim_fan_on = 0;
  return FAN_OFF_SUCCESS;
}

int get_temperature() {
  if (sim_fan_on) {
    if (sim_temp > 30) sim_temp--;
  } else {
    if (sim_temp < 80) sim_temp++;
  }
  return sim_temp;
}
