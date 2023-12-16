#include <gtk/gtk.h>
#include <pango/pango-font.h>
#include <stdio.h>
#include <stdbool.h>

#define ENTRY_FOR_TEMPERATURE 100
#define ENTRY_FOR_PIN 101

#define TEMPERATURE_ENTRY_INPUT_VALUE_ERROR -1
#define PIN_ENTRY_INPUT_VALUE_ERROR -2

void set_font_size(GtkWidget *widget, int size) {
  PangoFontDescription *font_desc = pango_font_description_new();
  pango_font_description_set_size(font_desc, size * PANGO_SCALE);
  gtk_widget_override_font(widget, font_desc);
  pango_font_description_free(font_desc);
}

int validate_input(char *value, int type) {
  int casted_value;  // cast char to int

  if (type ==  ENTRY_FOR_TEMPERATURE) {
    if (casted_value < 0 || casted_value > 100) return TEMPERATURE_ENTRY_INPUT_VALUE_ERROR;
  } else {
    if (casted_value < 0 || casted_value > 100) return PIN_ENTRY_INPUT_VALUE_ERROR; // depends on PIN numbers
  }
}

void handle_button_click(GtkButton *button, gpointer data) {
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *label_temperature;
  GtkWidget *temperature_entry;
  GtkWidget *label_pin;
  GtkWidget *pin_entry;

  label_temperature = gtk_label_new("Set temperature for the fan");
  temperature_entry = gtk_entry_new();

  label_pin = gtk_label_new("Set pin of the fan");
  pin_entry = gtk_entry_new();

  set_font_size(label_temperature, 13);
  set_font_size(label_pin, 13);

  dialog = gtk_dialog_new_with_buttons("Settings",
                                       NULL,
                                       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_STOCK_OK,
                                       GTK_RESPONSE_ACCEPT,
                                       GTK_STOCK_CANCEL,
                                       GTK_RESPONSE_REJECT,
                                       NULL);

  content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  gtk_container_add(GTK_CONTAINER(content_area), label_temperature);
  gtk_container_add(GTK_CONTAINER(content_area), temperature_entry);
  gtk_widget_show(label_temperature);
  gtk_widget_show(temperature_entry);

  gtk_container_add(GTK_CONTAINER(content_area), label_pin);
  gtk_container_add(GTK_CONTAINER(content_area), pin_entry);
  gtk_widget_show(label_pin);
  gtk_widget_show(pin_entry);

  gtk_entry_set_width_chars(GTK_ENTRY(temperature_entry), 3);
  
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

}

void switch_toggle(GtkSwitch *widget, gpointer data) {
  gboolean state = gtk_switch_get_state(widget);

  if (state == TRUE) {
    gtk_label_set_text(GTK_LABEL((GtkWidget *)data), "On");
  } else {
    /* gtk_label_set_text(GTK_LABEL(data), "Off"); */
  }
}

int main(int argc, char **argv) {

  GtkWidget *window;

  GtkWidget *button;
  /* GtkWidget *content_area; */

  GtkWidget *grid; 

  GtkWidget *toggle_fan;

  GtkWidget *label;
  GtkWidget *temperature_label;
  GtkWidget *celsius_label;
  GtkWidget *pin_label;
  GtkWidget *temperature_string_label;
  GtkWidget *pin_string_label;
  GtkWidget *fan_status_label;

  GtkWidget *temperature_entry;
  GtkWidget *pin_entry;

  GtkCssProvider *provider;
  GdkDisplay *display;
  GdkScreen *screen;

  char temperature_string[6];
  int temperature = 40;

  gtk_init(&argc, &argv);
  
  // initiation of CSS provider
  provider = gtk_css_provider_new();
  display = gdk_display_get_default();
  screen = gdk_display_get_default_screen(display);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Fan controller");
  gtk_container_set_border_width(GTK_CONTAINER(window), 50);
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  
  grid = gtk_grid_new();

  gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE); // Устанавливаем одинаковую высоту строк
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE); // Устанавливаем одинаковую ширину столбцов

  gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 15);

  button = gtk_button_new_with_label("Settings");

  toggle_fan = gtk_switch_new();
  sprintf(temperature_string, "%d%s", temperature, "°C");

  temperature_label = gtk_label_new(temperature_string);
  set_font_size(temperature_label, 35);

  /* GdkRGBA color_green; */
  /* GdkRGBA color_red; */
  /* gdk_rgba_parse(&color_green, "green"); */
  /* gdk_rgba_parse(&color_red, "red"); */


  /* gtk_widget_override_color(fan_status_label, GTK_STATE_FLAG_NORMAL, &color_green); */

  fan_status_label = gtk_label_new("Fan is on");

  const gchar *css = "label { color: green; }";
  gtk_css_provider_load_from_data(provider, css, -1, NULL);
  GtkStyleContext *context = gtk_widget_get_style_context(fan_status_label);
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  pin_string_label = gtk_label_new("Fan pin is 10");
  temperature_string_label = gtk_label_new("Fan is on at 40°C");

  set_font_size(fan_status_label, 15);
  set_font_size(pin_string_label, 15);
  set_font_size(temperature_string_label, 15);

  label = gtk_label_new("Off");
  
  celsius_label = gtk_label_new("°C");
  pin_label = gtk_label_new("fan pin");

  temperature_entry = gtk_entry_new();
  pin_entry = gtk_entry_new();

  gtk_widget_set_halign(fan_status_label, GTK_ALIGN_START);
  gtk_widget_set_halign(pin_string_label, GTK_ALIGN_START);
  gtk_widget_set_halign(temperature_string_label, GTK_ALIGN_START);

  gtk_widget_set_halign(temperature_label, GTK_ALIGN_CENTER);

  gtk_grid_attach(GTK_GRID(grid), temperature_label, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), fan_status_label, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), pin_string_label, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), temperature_string_label, 0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), button, 0, 4, 1, 1);
  
  gtk_entry_set_width_chars(GTK_ENTRY(temperature_entry), 3);
  gtk_entry_set_width_chars(GTK_ENTRY(pin_entry), 2);

  /* gtk_widget_set_sensitive(temperature_entry, false); */

  gtk_container_add(GTK_CONTAINER(window), grid);

  gtk_widget_show_all(window);

  g_signal_connect(toggle_fan, "state-set", G_CALLBACK(switch_toggle), label);
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(GTK_BUTTON(button), "clicked", G_CALLBACK(handle_button_click), "123");
  
  gtk_main();
  return 0;
}
