#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>
#include "raspberry.h"

#define SOCKET_PATH    "/tmp/fan-controller.sock"
#define MAX_CLIENTS    8
#define TICK_MS        1000
#define HYSTERESIS_C   5
#define LINE_BUF       256
#define DEFAULT_TEMP_C 40
#define DEFAULT_PIN    4

typedef struct {
  int    fd;
  char   buf[LINE_BUF];
  size_t len;
} client;

typedef struct {
  int temp_threshold;
  int pin;
  int fan_on;
  int last_temp;
} daemon_state;

static volatile sig_atomic_t shutdown_requested = 0;

static void on_signal(int sig) {
  (void)sig;
  shutdown_requested = 1;
}

static int create_listener(const char *path) {
  unlink(path);

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) { perror("socket"); return -1; }

  struct sockaddr_un addr = {0};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(fd);
    return -1;
  }

  if (listen(fd, 4) < 0) {
    perror("listen");
    close(fd);
    return -1;
  }

  chmod(path, 0660);
  return fd;
}

static void send_str(int fd, const char *s) {
  size_t len = strlen(s);
  ssize_t n = write(fd, s, len);
  (void)n;
}

static void handle_line(daemon_state *state, int client_fd, const char *line) {
  char cmd[16] = {0}, target[16] = {0};
  int value = 0;
  int n = sscanf(line, "%15s %15s %d", cmd, target, &value);

  if (n >= 2 && strcmp(cmd, "GET") == 0 && strcmp(target, "state") == 0) {
    char reply[128];
    snprintf(reply, sizeof(reply),
             "temp=%d threshold=%d pin=%d fan=%s\n",
             state->last_temp, state->temp_threshold, state->pin,
             state->fan_on ? "on" : "off");
    send_str(client_fd, reply);
    return;
  }

  if (n == 3 && strcmp(cmd, "SET") == 0) {
    if (strcmp(target, "temp") == 0) {
      if (value < 0 || value > 100) {
        send_str(client_fd, "ERR temp out of range\n");
        return;
      }
      state->temp_threshold = value;
      send_str(client_fd, "OK\n");
      return;
    }
    if (strcmp(target, "pin") == 0) {
      if (value < 0 || value > 27) {
        send_str(client_fd, "ERR pin out of range\n");
        return;
      }
      state->pin = value;
      send_str(client_fd, "OK\n");
      return;
    }
  }

  send_str(client_fd, "ERR unknown command\n");
}

static void process_client_buffer(daemon_state *state, client *c) {
  while (1) {
    char *nl = memchr(c->buf, '\n', c->len);
    if (!nl) break;
    *nl = '\0';
    handle_line(state, c->fd, c->buf);
    size_t consumed = (size_t)(nl - c->buf) + 1;
    memmove(c->buf, c->buf + consumed, c->len - consumed);
    c->len -= consumed;
  }
}

static void tick(daemon_state *state) {
  state->last_temp = get_temperature();

  if (!state->fan_on && state->last_temp >= state->temp_threshold) {
    if (turn_on_fan(state->pin) == FAN_ON_SUCCESS) {
      state->fan_on = 1;
      fprintf(stderr, "fanctld: fan ON  (temp=%d threshold=%d pin=%d)\n",
              state->last_temp, state->temp_threshold, state->pin);
    }
  } else if (state->fan_on &&
             state->last_temp <= state->temp_threshold - HYSTERESIS_C) {
    if (turn_off_fan(state->pin) == FAN_OFF_SUCCESS) {
      state->fan_on = 0;
      fprintf(stderr, "fanctld: fan OFF (temp=%d threshold=%d pin=%d)\n",
              state->last_temp, state->temp_threshold, state->pin);
    }
  }
}

int main(void) {
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGPIPE, SIG_IGN);

  daemon_state state = {
    .temp_threshold = DEFAULT_TEMP_C,
    .pin            = DEFAULT_PIN,
    .fan_on         = 0,
    .last_temp      = 0,
  };

  int listen_fd = create_listener(SOCKET_PATH);
  if (listen_fd < 0) return 1;

  client clients[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].fd = -1;
    clients[i].len = 0;
  }

  fprintf(stderr, "fanctld: listening on %s (threshold=%d°C, pin=%d)\n",
          SOCKET_PATH, state.temp_threshold, state.pin);

  tick(&state);

  while (!shutdown_requested) {
    struct pollfd pfds[1 + MAX_CLIENTS];
    pfds[0].fd = listen_fd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;

    int nfds = 1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].fd >= 0) {
        pfds[nfds].fd = clients[i].fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;
        nfds++;
      }
    }

    int ready = poll(pfds, nfds, TICK_MS);
    if (ready < 0) {
      if (errno == EINTR) continue;
      perror("poll");
      break;
    }

    if (ready == 0) {
      tick(&state);
      continue;
    }

    if (pfds[0].revents & POLLIN) {
      int cfd = accept(listen_fd, NULL, NULL);
      if (cfd >= 0) {
        int placed = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i].fd < 0) {
            clients[i].fd = cfd;
            clients[i].len = 0;
            placed = 1;
            break;
          }
        }
        if (!placed) close(cfd);
      }
    }

    int idx = 1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].fd < 0) continue;
      short revents = pfds[idx].revents;
      idx++;

      if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
        close(clients[i].fd);
        clients[i].fd = -1;
        clients[i].len = 0;
        continue;
      }

      if (revents & POLLIN) {
        ssize_t n = read(clients[i].fd,
                         clients[i].buf + clients[i].len,
                         sizeof(clients[i].buf) - clients[i].len);
        if (n <= 0) {
          close(clients[i].fd);
          clients[i].fd = -1;
          clients[i].len = 0;
          continue;
        }
        clients[i].len += (size_t)n;
        process_client_buffer(&state, &clients[i]);

        if (clients[i].len == sizeof(clients[i].buf)) {
          send_str(clients[i].fd, "ERR line too long\n");
          close(clients[i].fd);
          clients[i].fd = -1;
          clients[i].len = 0;
        }
      }
    }
  }

  fprintf(stderr, "fanctld: shutting down\n");
  if (state.fan_on) turn_off_fan(state.pin);
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].fd >= 0) close(clients[i].fd);
  }
  close(listen_fd);
  unlink(SOCKET_PATH);
  return 0;
}
