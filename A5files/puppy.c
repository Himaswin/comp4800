#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <math.h>

// Global variables
static GList *redo_stack = NULL;
static GtkWidget *color_display, *picture;
static GdkPixbuf *pixbuf = NULL;
static cairo_surface_t *drawable_surface = NULL;
static gboolean painting_mode = FALSE;
static GdkRGBA selected_color;
static GList *undo_stack = NULL;


// Forward declaration of helper functions
void gdk_pixbuf_get_pixel_at_rgba(GdkPixbuf *pixbuf, int x, int y, GdkRGBA *color);
static void save_current_state(void);
static void undo_last_action(void);
static void update_picture(GtkWidget *picture);
static void save_image(GtkWidget *widget, gpointer data);
static void save_current_state_to_redo_stack(void);

// Function to draw on the surface
static void draw_on_surface(cairo_surface_t *surface, double x, double y, GdkRGBA color) {
    // Save current state before drawing
    save_current_state();

    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    double radius = 5.0; // Brush size
    cairo_arc(cr, x, y, radius, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_destroy(cr);

    update_picture(picture);
}

// Function to update the picture with the drawable surface
static void update_picture(GtkWidget *picture) {
    GdkPixbuf *new_pixbuf = gdk_pixbuf_get_from_surface(drawable_surface, 0, 0,
                                                        cairo_image_surface_get_width(drawable_surface),
                                                        cairo_image_surface_get_height(drawable_surface));
    gtk_picture_set_pixbuf(GTK_PICTURE(picture), new_pixbuf);
    g_object_unref(new_pixbuf);
}

// Function to update the color display
static void update_color_display(GdkRGBA color) {
    char *color_str = gdk_rgba_to_string(&color);
    GtkCssProvider *provider = gtk_css_provider_new();
    char *css = g_strdup_printf("* { background-color: %s; }", color_str);
    gtk_css_provider_load_from_data(provider, css, -1);
    gtk_style_context_add_provider(gtk_widget_get_style_context(color_display),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_free(color_str);
    g_free(css);
    g_object_unref(provider);
}

// Callback function for mouse click on the picture
static gboolean picture_click_callback(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, GtkWidget *picture) {
    if (painting_mode && drawable_surface != NULL) {
        draw_on_surface(drawable_surface, x, y, selected_color);
        return TRUE;
    }

    if (pixbuf == NULL) {
        return FALSE;
    }

    GdkRGBA color;
    gdk_pixbuf_get_pixel_at_rgba(pixbuf, x, y, &color);

    // Print the RGBA values of the clicked pixel
    g_print("Clicked Pixel RGBA: (%f, %f, %f, %f)\n", color.red, color.green, color.blue, color.alpha);

    if (painting_mode && drawable_surface != NULL) {
        draw_on_surface(drawable_surface, x, y, selected_color);
        update_picture(picture);
        return TRUE;
    }


    selected_color = color;
    update_color_display(color);

    return TRUE;
}

// Callback functions for "Pick", "Paint", and "Undo" actions
static void pick_activate(GtkWidget *widget, gpointer user_data) {
    g_print("Pick action activated\n");
    painting_mode = FALSE;
}

static void paint_activate(GtkWidget *widget, gpointer user_data) {
    g_print("Paint action activated\n");
    painting_mode = TRUE;
}

static void undo_activate(GtkWidget *widget, gpointer user_data) {
    undo_last_action();
}

// Function to save the current state of the drawable surface
static void save_current_state(void) {
    cairo_surface_t *state = cairo_surface_create_similar(drawable_surface,
                                                          cairo_surface_get_content(drawable_surface),
                                                          cairo_image_surface_get_width(drawable_surface),
                                                          cairo_image_surface_get_height(drawable_surface));
    cairo_t *cr = cairo_create(state);
    cairo_set_source_surface(cr, drawable_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    undo_stack = g_list_prepend(undo_stack, state);
}

// Function to undo the last action
static void undo_last_action(void) {
    if (undo_stack == NULL) {
        g_print("Nothing to undo\n");
        return;
    }

    // Save the current state to the redo stack
    save_current_state_to_redo_stack();

    cairo_surface_destroy(drawable_surface);
    drawable_surface = (cairo_surface_t *)undo_stack->data;
    undo_stack = g_list_delete_link(undo_stack, undo_stack);

    update_picture(picture);
}

// Function to redo the last action
static void redo_last_action(void) {
    if (redo_stack == NULL) {
        g_print("Nothing to redo\n");
        return;
    }

    cairo_surface_destroy(drawable_surface);
    drawable_surface = (cairo_surface_t *)redo_stack->data;
    redo_stack = g_list_delete_link(redo_stack, redo_stack);

    update_picture(picture);
}

// Function to save
static void save_current_state_to_redo_stack(void) {
    cairo_surface_t *state = cairo_surface_create_similar(drawable_surface,
                                                          cairo_surface_get_content(drawable_surface),
                                                          cairo_image_surface_get_width(drawable_surface),
                                                          cairo_image_surface_get_height(drawable_surface));
    cairo_t *cr = cairo_create(state);
    cairo_set_source_surface(cr, drawable_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    redo_stack = g_list_prepend(redo_stack, state);
}


// Function for main application window setup
static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window, *button_box, *pick_button, *paint_button, *undo_button, *header_bar;

    // Create a new window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Puppy");

    // Load the image into a pixbuf
    pixbuf = gdk_pixbuf_new_from_file("./Lucky.png", NULL);
    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);

    // Create header bar and add save button
    header_bar = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    GtkWidget *save_button = gtk_button_new_with_label("Save");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), save_button);
    g_signal_connect(save_button, "clicked", G_CALLBACK(save_image), NULL);

    // Create a drawable surface from the pixbuf
    cairo_t *cr;
    drawable_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(drawable_surface);
    gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    // Set the window size
    gtk_window_set_default_size(GTK_WINDOW(window), width + 20, height + 80);

    // Create vertical box for layout
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    // Create horizontal box for buttons and color display
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(vbox), button_box);

    // Create "Pick" button
    pick_button = gtk_button_new_with_label("Pick");
    g_signal_connect(pick_button, "clicked", G_CALLBACK(pick_activate), NULL);
    gtk_box_append(GTK_BOX(button_box), pick_button);

    // Create "Paint" button
    paint_button = gtk_button_new_with_label("Paint");
    g_signal_connect(paint_button, "clicked", G_CALLBACK(paint_activate), NULL);
    gtk_box_append(GTK_BOX(button_box), paint_button);

    // Create "Undo" button
    undo_button = gtk_button_new_with_label("Undo");
    g_signal_connect(undo_button, "clicked", G_CALLBACK(undo_activate), NULL);
    gtk_box_append(GTK_BOX(button_box), undo_button);

    GtkWidget *redo_button = gtk_button_new_with_label("Redo");
    g_signal_connect(redo_button, "clicked", G_CALLBACK(redo_last_action), NULL);
    gtk_box_append(GTK_BOX(button_box), redo_button);


    // Create widget for color display
    color_display = gtk_label_new(NULL);
    gtk_widget_set_size_request(color_display, 50, 50);
    gtk_box_append(GTK_BOX(button_box), color_display);

    // Create picture widget for the image
    picture = gtk_picture_new_for_pixbuf(pixbuf);
    gtk_picture_set_keep_aspect_ratio(GTK_PICTURE(picture), TRUE);
    gtk_widget_set_hexpand(picture, FALSE);
    gtk_widget_set_vexpand(picture, FALSE);
    gtk_box_append(GTK_BOX(vbox), picture);

    // Add click event handler to the picture
    GtkGesture *click_gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_gesture), GDK_BUTTON_PRIMARY);
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(picture_click_callback), picture);
    gtk_widget_add_controller(picture, GTK_EVENT_CONTROLLER(click_gesture));

    // Show the window
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}

// Callback function for saving the image
static void save_image(GtkWidget *widget, gpointer data) {
    if (drawable_surface == NULL) {
        g_print("No image to save.\n");
        return;
    }

    const char *file_path = "saved_image.png";
    cairo_status_t status = cairo_surface_write_to_png(drawable_surface, file_path);

    if (status != CAIRO_STATUS_SUCCESS) {
        g_printerr("Failed to save image: %s\n", cairo_status_to_string(status));
    } else {
        g_print("Image saved successfully to %s\n", file_path);
    }
}

// Helper function to get the pixel color
void gdk_pixbuf_get_pixel_at_rgba(GdkPixbuf *pixbuf, int x, int y, GdkRGBA *color) {
    int width, height, rowstride, n_channels;
    guchar *pixels, *p;

    n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    if (!gdk_pixbuf_get_has_alpha(pixbuf)) {
        g_print("The image must have an alpha channel\n");
    }

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    pixels = gdk_pixbuf_get_pixels(pixbuf);

    p = pixels + y * rowstride + x * n_channels;
    color->red = (gdouble)p[0] / 255.0;
    color->green = (gdouble)p[1] / 255.0;
    color->blue = (gdouble)p[2] / 255.0;
    color->alpha = (gdouble)p[3] / 255.0;
}
