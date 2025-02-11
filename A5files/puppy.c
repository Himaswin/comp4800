#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <math.h>

// Global variables
static GList *redo_stack = NULL;
static GtkWidget *color_display, *color_label, *picture;
static GdkPixbuf *pixbuf = NULL;
static cairo_surface_t *drawable_surface = NULL;
static gboolean painting_mode = FALSE;
static GdkRGBA selected_color;
static GList *undo_stack = NULL;
static GtkCssProvider *provider = NULL;

// Forward declarations
void gdk_pixbuf_get_pixel_at_rgba(GdkPixbuf *pixbuf, int x, int y, GdkRGBA *color);
static void save_current_state(void);
static void undo_last_action(void);
static void update_picture(void);
static void save_image(GtkWidget *widget, gpointer data);
static void cleanup_surfaces(void);

static void draw_color_display(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    cairo_set_source_rgba(cr, selected_color.red, selected_color.green, selected_color.blue, selected_color.alpha);
    cairo_rectangle(cr, 1, 1, width-2, height-2);
    cairo_fill(cr);
}

static void update_color_display(GdkRGBA color) {
    selected_color = color;
    gtk_widget_queue_draw(color_display);
    
    char color_text[32];
    snprintf(color_text, sizeof(color_text), "RGB: %d,%d,%d",
             (int)(color.red * 255),
             (int)(color.green * 255),
             (int)(color.blue * 255));
    gtk_label_set_text(GTK_LABEL(color_label), color_text);
}

static void draw_on_surface(cairo_surface_t *surface, double x, double y, GdkRGBA color) {
    save_current_state();

    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    double radius = 5.0;
    cairo_arc(cr, x, y, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_destroy(cr);

    update_picture();
}

static void update_picture(void) {
    GdkPixbuf *new_pixbuf = gdk_pixbuf_get_from_surface(drawable_surface, 0, 0,
                                                        cairo_image_surface_get_width(drawable_surface),
                                                        cairo_image_surface_get_height(drawable_surface));
    gtk_picture_set_paintable(GTK_PICTURE(picture), GDK_PAINTABLE(gdk_texture_new_for_pixbuf(new_pixbuf)));
    g_object_unref(new_pixbuf);
}

static void save_current_state(void) {
    cairo_surface_t *state = cairo_surface_create_similar(
        drawable_surface,
        cairo_surface_get_content(drawable_surface),
        cairo_image_surface_get_width(drawable_surface),
        cairo_image_surface_get_height(drawable_surface)
    );
    
    cairo_t *cr = cairo_create(state);
    cairo_set_source_surface(cr, drawable_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    // Clear redo stack when new action is performed
    while (redo_stack != NULL) {
        cairo_surface_destroy((cairo_surface_t *)redo_stack->data);
        redo_stack = g_list_delete_link(redo_stack, redo_stack);
    }

    undo_stack = g_list_prepend(undo_stack, state);
}

static void undo_last_action(void) {
    if (undo_stack == NULL) {
        g_print("Nothing to undo\n");
        return;
    }

    // Save current state to redo stack
    cairo_surface_t *current_state = cairo_surface_create_similar(
        drawable_surface,
        cairo_surface_get_content(drawable_surface),
        cairo_image_surface_get_width(drawable_surface),
        cairo_image_surface_get_height(drawable_surface)
    );
    
    cairo_t *cr = cairo_create(current_state);
    cairo_set_source_surface(cr, drawable_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    
    redo_stack = g_list_prepend(redo_stack, current_state);

    // Restore previous state
    cairo_surface_t *prev_state = (cairo_surface_t *)undo_stack->data;
    undo_stack = g_list_delete_link(undo_stack, undo_stack);

    // Copy previous state to drawable surface
    cr = cairo_create(drawable_surface);
    cairo_set_source_surface(cr, prev_state, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    cairo_surface_destroy(prev_state);
    update_picture();
}

static void redo_last_action(void) {
    if (redo_stack == NULL) {
        g_print("Nothing to redo\n");
        return;
    }

    // Save current state to undo stack
    cairo_surface_t *current_state = cairo_surface_create_similar(
        drawable_surface,
        cairo_surface_get_content(drawable_surface),
        cairo_image_surface_get_width(drawable_surface),
        cairo_image_surface_get_height(drawable_surface)
    );
    
    cairo_t *cr = cairo_create(current_state);
    cairo_set_source_surface(cr, drawable_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    
    undo_stack = g_list_prepend(undo_stack, current_state);

    // Restore redo state
    cairo_surface_t *redo_state = (cairo_surface_t *)redo_stack->data;
    redo_stack = g_list_delete_link(redo_stack, redo_stack);

    // Copy redo state to drawable surface
    cr = cairo_create(drawable_surface);
    cairo_set_source_surface(cr, redo_state, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    cairo_surface_destroy(redo_state);
    update_picture();
}

static void picture_click_callback(GtkGestureClick *gesture, int n_press, double x, double y, GtkWidget *picture) {
    if (painting_mode && drawable_surface != NULL) {
        draw_on_surface(drawable_surface, x, y, selected_color);
        return;
    }

    if (pixbuf != NULL) {
        GdkRGBA color;
        gdk_pixbuf_get_pixel_at_rgba(pixbuf, x, y, &color);
        g_print("Clicked Pixel RGBA: (%f, %f, %f, %f)\n", color.red, color.green, color.blue, color.alpha);
        selected_color = color;
        update_color_display(color);
    }
}

static void pick_activate(GtkWidget *widget, gpointer user_data) {
    painting_mode = FALSE;
    g_print("Pick mode activated\n");
}

static void paint_activate(GtkWidget *widget, gpointer user_data) {
    painting_mode = TRUE;
    g_print("Paint mode activated\n");
}

static void cleanup_surfaces(void) {
    while (undo_stack != NULL) {
        cairo_surface_destroy((cairo_surface_t *)undo_stack->data);
        undo_stack = g_list_delete_link(undo_stack, undo_stack);
    }

    while (redo_stack != NULL) {
        cairo_surface_destroy((cairo_surface_t *)redo_stack->data);
        redo_stack = g_list_delete_link(redo_stack, redo_stack);
    }

    if (drawable_surface) {
        cairo_surface_destroy(drawable_surface);
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Paint Application");

    pixbuf = gdk_pixbuf_new_from_file("Lucky.png", NULL);
    if (!pixbuf) {
        g_print("Failed to load image\n");
        return;
    }

    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);

    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    GtkWidget *save_button = gtk_button_new_with_label("Save");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), save_button);
    g_signal_connect(save_button, "clicked", G_CALLBACK(save_image), NULL);

    drawable_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(drawable_surface);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    gtk_window_set_default_size(GTK_WINDOW(window), width + 20, height + 80);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(vbox), button_box);

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "drawingarea {"
        "  border: 1px solid #999;"
        "  border-radius: 4px;"
        "  margin: 2px;"
        "}", -1);

    GtkWidget *pick_button = gtk_button_new_with_label("Pick");
    GtkWidget *paint_button = gtk_button_new_with_label("Paint");
    GtkWidget *undo_button = gtk_button_new_with_label("Undo");
    GtkWidget *redo_button = gtk_button_new_with_label("Redo");

    color_display = gtk_drawing_area_new();
    gtk_widget_set_size_request(color_display, 50, 30);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(color_display), draw_color_display, NULL, NULL);

    gtk_widget_add_css_class(color_display, "color-display");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *color_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(color_box), color_display);

    color_label = gtk_label_new("RGB: ---");
    gtk_widget_set_size_request(color_label, 80, -1);
    gtk_box_append(GTK_BOX(color_box), color_label);

    g_signal_connect(pick_button, "clicked", G_CALLBACK(pick_activate), NULL);
    g_signal_connect(paint_button, "clicked", G_CALLBACK(paint_activate), NULL);
    g_signal_connect(undo_button, "clicked", G_CALLBACK(undo_last_action), NULL);
    g_signal_connect(redo_button, "clicked", G_CALLBACK(redo_last_action), NULL);

    gtk_box_append(GTK_BOX(button_box), pick_button);
    gtk_box_append(GTK_BOX(button_box), paint_button);
    gtk_box_append(GTK_BOX(button_box), undo_button);
    gtk_box_append(GTK_BOX(button_box), redo_button);
    gtk_box_append(GTK_BOX(button_box), color_box);

    picture = gtk_picture_new_for_pixbuf(pixbuf);
    gtk_picture_set_keep_aspect_ratio(GTK_PICTURE(picture), TRUE);
    gtk_widget_set_hexpand(picture, FALSE);
    gtk_widget_set_vexpand(picture, FALSE);
    gtk_box_append(GTK_BOX(vbox), picture);

    GtkGesture *click_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), GDK_BUTTON_PRIMARY);
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(picture_click_callback), picture);
    gtk_widget_add_controller(picture, GTK_EVENT_CONTROLLER(click_gesture));

    gtk_window_present(GTK_WINDOW(window));
}

static void save_image(GtkWidget *widget, gpointer data) {
    if (drawable_surface == NULL) {
        g_print("No image to save.\n");
        return;
    }

    cairo_status_t status = cairo_surface_write_to_png(drawable_surface, "saved_image.png");
    if (status != CAIRO_STATUS_SUCCESS) {
        g_printerr("Failed to save image: %s\n", cairo_status_to_string(status));
    } else {
        g_print("Image saved successfully to saved_image.png\n");
    }
}

void gdk_pixbuf_get_pixel_at_rgba(GdkPixbuf *pixbuf, int x, int y, GdkRGBA *color) {
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    guchar *p = pixels + y * rowstride + x * n_channels;
    color->red = (gdouble)p[0] / 255.0;
    color->green = (gdouble)p[1] / 255.0;
    color->blue = (gdouble)p[2] / 255.0;
    color->alpha = n_channels > 3 ? (gdouble)p[3] / 255.0 : 1.0;
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    cleanup_surfaces();
    if (provider) {
        g_object_unref(provider);
    }
    if (pixbuf) {
        g_object_unref(pixbuf);
    }
    g_object_unref(app);
    
    return status;
}
