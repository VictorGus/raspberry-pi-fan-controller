#include "raspberry.h"
#include <stdio.h>
#include <gpiod.h>

#define GPIO_CHIP_PATH    "/dev/gpiochip0"
#define THERMAL_ZONE_PATH "/sys/class/thermal/thermal_zone0/temp"
#define CONSUMER          "fanctld"

static struct gpiod_chip *chip = NULL;
static struct gpiod_line *line = NULL;
static int current_pin = -1;

static int ensure_line(int pin) {
  if (pin == current_pin && line != NULL) return 0;

  if (line) {
    gpiod_line_release(line);
    line = NULL;
  }

  if (!chip) {
    chip = gpiod_chip_open(GPIO_CHIP_PATH);
    if (!chip) {
      perror("gpiod_chip_open");
      return -1;
    }
  }

  line = gpiod_chip_get_line(chip, (unsigned int)pin);
  if (!line) {
    perror("gpiod_chip_get_line");
    return -1;
  }

  if (gpiod_line_request_output(line, CONSUMER, 0) < 0) {
    perror("gpiod_line_request_output");
    line = NULL;
    return -1;
  }

  current_pin = pin;
  return 0;
}

int get_fan_pin(void) {
  return 4;
}

int turn_on_fan(int pin) {
  if (ensure_line(pin) < 0) return FAN_ON_FAILURE;
  if (gpiod_line_set_value(line, 1) < 0) {
    perror("gpiod_line_set_value");
    return FAN_ON_FAILURE;
  }
  return FAN_ON_SUCCESS;
}

int turn_off_fan(int pin) {
  if (ensure_line(pin) < 0) return FAN_OFF_FAILURE;
  if (gpiod_line_set_value(line, 0) < 0) {
    perror("gpiod_line_set_value");
    return FAN_OFF_FAILURE;
  }
  return FAN_OFF_SUCCESS;
}

int get_temperature(void) {
  FILE *f = fopen(THERMAL_ZONE_PATH, "r");
  if (!f) {
    perror("open thermal_zone");
    return -1;
  }
  int millideg = 0;
  int n = fscanf(f, "%d", &millideg);
  fclose(f);
  if (n != 1) return -1;
  return millideg / 1000;
}
