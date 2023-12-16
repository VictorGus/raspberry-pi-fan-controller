#ifndef FAN
#define FAN

#define FAN_NOT_FOUMD_ERROR -1
#define UNKNOWN_FAN_ERROR -2
#define FAN_OFF_SUCCESS 0
#define FAN_OFF_FAILURE -3
#define FAN_ON_SUCCESS 1
#define FAN_ON_FAILURE -4

int get_fan_pin();
int turn_on_fan(int pin);
int turn_off_fan(int pin);
int get_temperature();

#endif
