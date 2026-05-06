#include <gtk/gtk.h>
#include <pango/pango-font.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#define DAEMON_SOCKET_PATH "/tmp/fan-controller.sock"
#define POLL_INTERVAL_MS   1000
#define IO_TIMEOUT_MS      100

static const int VALID_PI3_PINS[] = {
  4, 5, 6, 12, 13, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27
};
#define VALID_PI3_PINS_COUNT (sizeof(VALID_PI3_PINS) / sizeof(VALID_PI3_PINS[0]))

typedef struct {
  int connected;
  int temp;
  int threshold;
  int pin;
  int fan_on;
} state_snapshot;

typedef struct {
  GtkLabel *temperature;
  GtkLabel *fan_status;
  GtkLabel *pin_string;
  GtkLabel *threshold_string;
} ui_labels;

void set_font_size(GtkWidget *widget, int size) {
  PangoFontDescription *font_desc = pango_font_description_new();
  pango_font_description_set_size(font_desc, size * PANGO_SCALE);
  gtk_widget_override_font(widget, font_desc);
  pango_font_description_free(font_desc);
}

static int daemon_query(const char *cmd, char *reply, size_t reply_size) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return 0;

  struct timeval tv = {.tv_sec = 0, .tv_usec = IO_TIMEOUT_MS * 1000};
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  struct sockaddr_un addr = {0};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, DAEMON_SOCKET_PATH, sizeof(addr.sun_path) - 1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return 0;
  }

  size_t cmd_len = strlen(cmd);
  if (write(fd, cmd, cmd_len) != (ssize_t)cmd_len) {
    close(fd);
    return 0;
  }

  size_t total = 0;
  while (total < reply_size - 1) {
    ssize_t n = read(fd, reply + total, reply_size - 1 - total);
    if (n <= 0) break;
    total += (size_t)n;
    if (memchr(reply, '\n', total)) break;
  }
  reply[total] = '\0';

  close(fd);
  return total > 0;
}

static state_snapshot fetch_state(void) {
  state_snapshot s = {0};
  char buf[256];
  if (!daemon_query("GET state\n", buf, sizeof(buf))) return s;

  char fan[8] = {0};
  int n = sscanf(buf, "temp=%d threshold=%d pin=%d fan=%7s",
                 &s.temp, &s.threshold, &s.pin, fan);
  if (n == 4) {
    s.connected = 1;
    s.fan_on = (strcmp(fan, "on") == 0);
  }
  return s;
}

static void set_status_class(GtkWidget *w, const char *cls) {
  GtkStyleContext *ctx = gtk_widget_get_style_context(w);
  gtk_style_context_remove_class(ctx, "fan-on");
  gtk_style_context_remove_class(ctx, "fan-off");
  gtk_style_context_remove_class(ctx, "disconnected");
  gtk_style_context_add_class(ctx, cls);
}

static gboolean tick_state(gpointer user_data) {
  ui_labels *u = user_data;
  state_snapshot s = fetch_state();
  char buf[64];

  if (!s.connected) {
    gtk_label_set_text(u->temperature, "—");
    gtk_label_set_text(u->fan_status, "Disconnected");
    set_status_class(GTK_WIDGET(u->fan_status), "disconnected");
    gtk_label_set_text(u->pin_string, "");
    gtk_label_set_text(u->threshold_string, "");
    return G_SOURCE_CONTINUE;
  }

  snprintf(buf, sizeof(buf), "%d°C", s.temp);
  gtk_label_set_text(u->temperature, buf);

  gtk_label_set_text(u->fan_status, s.fan_on ? "Fan is on" : "Fan is off");
  set_status_class(GTK_WIDGET(u->fan_status), s.fan_on ? "fan-on" : "fan-off");

  snprintf(buf, sizeof(buf), "Fan pin is %d", s.pin);
  gtk_label_set_text(u->pin_string, buf);

  snprintf(buf, sizeof(buf), "Fan turns on at %d°C", s.threshold);
  gtk_label_set_text(u->threshold_string, buf);

  return G_SOURCE_CONTINUE;
}

void handle_button_click(GtkButton *button, gpointer data) {
  (void)data;

  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *label_temperature;
  GtkWidget *temperature_spin;
  GtkWidget *label_pin;
  GtkWidget *pin_combo;

  state_snapshot current = fetch_state();

  label_temperature = gtk_label_new("Temperature threshold (°C)");
  temperature_spin = gtk_spin_button_new_with_range(0.0, 100.0, 1.0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(temperature_spin),
                            current.connected ? current.threshold : 40);

  label_pin = gtk_label_new("Fan pin (BCM)");
  pin_combo = gtk_combo_box_text_new();
  int initial_pin_index = 0;
  for (size_t i = 0; i < VALID_PI3_PINS_COUNT; i++) {
    char text[8];
    snprintf(text, sizeof(text), "%d", VALID_PI3_PINS[i]);
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pin_combo), text);
    if (current.connected && VALID_PI3_PINS[i] == current.pin) {
      initial_pin_index = (int)i;
    }
  }
  gtk_combo_box_set_active(GTK_COMBO_BOX(pin_combo), initial_pin_index);

  set_font_size(label_temperature, 13);
  set_font_size(label_pin, 13);

  GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  dialog = gtk_dialog_new_with_buttons("Settings",
                                       parent,
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_STOCK_OK,
                                       GTK_RESPONSE_ACCEPT,
                                       GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_REJECT,
                                       NULL);

  content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(content_area), 10);
  gtk_box_set_spacing(GTK_BOX(content_area), 6);

  gtk_container_add(GTK_CONTAINER(content_area), label_temperature);
  gtk_container_add(GTK_CONTAINER(content_area), temperature_spin);
  gtk_container_add(GTK_CONTAINER(content_area), label_pin);
  gtk_container_add(GTK_CONTAINER(content_area), pin_combo);

  gtk_widget_show_all(dialog);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    int temp_value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(temperature_spin));
    int pin_index = gtk_combo_box_get_active(GTK_COMBO_BOX(pin_combo));
    int pin_value = VALID_PI3_PINS[pin_index];

    char cmd[64], reply[64];
    snprintf(cmd, sizeof(cmd), "SET temp %d\n", temp_value);
    daemon_query(cmd, reply, sizeof(reply));
    snprintf(cmd, sizeof(cmd), "SET pin %d\n", pin_value);
    daemon_query(cmd, reply, sizeof(reply));
  }

  gtk_widget_destroy(dialog);
}

int main(int argc, char **argv) {
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *grid;

  GtkWidget *temperature_label;
  GtkWidget *temperature_string_label;
  GtkWidget *pin_string_label;
  GtkWidget *fan_status_label;

  GtkCssProvider *provider;
  GdkDisplay *display;
  GdkScreen *screen;

  gtk_init(&argc, &argv);

  provider = gtk_css_provider_new();
  display = gdk_display_get_default();
  screen = gdk_display_get_default_screen(display);

  const gchar *css =
    ".fan-on { color: #2ecc40; font-weight: bold; }"
    ".fan-off { color: #888888; }"
    ".disconnected { color: #ff4136; font-style: italic; }";
  gtk_css_provider_load_from_data(provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen(screen,
                                            GTK_STYLE_PROVIDER(provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Fan controller");
  gtk_container_set_border_width(GTK_CONTAINER(window), 50);
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

  grid = gtk_grid_new();
  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 15);

  button = gtk_button_new_with_label("Settings");

  temperature_label = gtk_label_new("—");
  set_font_size(temperature_label, 35);

  fan_status_label = gtk_label_new("Connecting…");
  set_font_size(fan_status_label, 15);

  pin_string_label = gtk_label_new("");
  temperature_string_label = gtk_label_new("");
  set_font_size(pin_string_label, 15);
  set_font_size(temperature_string_label, 15);

  gtk_widget_set_halign(fan_status_label, GTK_ALIGN_START);
  gtk_widget_set_halign(pin_string_label, GTK_ALIGN_START);
  gtk_widget_set_halign(temperature_string_label, GTK_ALIGN_START);
  gtk_widget_set_halign(temperature_label, GTK_ALIGN_CENTER);

  gtk_grid_attach(GTK_GRID(grid), temperature_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), fan_status_label, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), pin_string_label, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), temperature_string_label, 0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), button, 0, 4, 1, 1);

  gtk_container_add(GTK_CONTAINER(window), grid);
  gtk_widget_show_all(window);

  static ui_labels labels;
  labels.temperature = GTK_LABEL(temperature_label);
  labels.fan_status = GTK_LABEL(fan_status_label);
  labels.pin_string = GTK_LABEL(pin_string_label);
  labels.threshold_string = GTK_LABEL(temperature_string_label);

  tick_state(&labels);
  g_timeout_add(POLL_INTERVAL_MS, tick_state, &labels);

  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(GTK_BUTTON(button), "clicked", G_CALLBACK(handle_button_click), NULL);

  gtk_main();
  return 0;
}
