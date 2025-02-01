#include <gtk/gtk.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *image;
    GdkPixbuf *pixbuf;
    GtkWidget *color_window;
} AppData;

// Forward declaration of draw_color function
static void draw_color(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);

static void get_pixel_color(GdkPixbuf *pixbuf, int x, int y, guchar *r, guchar *g, guchar *b) {
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    int stride = gdk_pixbuf_get_rowstride(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);
    
    guchar *p = pixels + y * stride + x * channels;
    *r = p[0];
    *g = p[1];
    *b = p[2];
}

// Define draw_color function
static void draw_color(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    GdkRGBA *color = (GdkRGBA *)user_data;
    gdk_cairo_set_source_rgba(cr, color);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
}

static void show_color_window(AppData *app_data, guchar r, guchar g, guchar b) {
    if (app_data->color_window != NULL) {
        gtk_window_destroy(GTK_WINDOW(app_data->color_window));
    }

    app_data->color_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(app_data->color_window), "Color Display");
    gtk_window_set_default_size(GTK_WINDOW(app_data->color_window), 100, 100);

    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_window_set_child(GTK_WINDOW(app_data->color_window), drawing_area);

    // Create color structure and allocate it dynamically
    GdkRGBA *color = g_new(GdkRGBA, 1);
    color->red = r / 255.0;
    color->green = g / 255.0;
    color->blue = b / 255.0;
    color->alpha = 1.0;

    gtk_widget_set_size_request(drawing_area, 100, 100);

    // Set up drawing function for the drawing area
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), 
        draw_color, color, g_free);

    gtk_widget_show(app_data->color_window);
}

static void on_image_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer data) {
    AppData *app_data = (AppData *)data;
    int ix = (int)x;
    int iy = (int)y;

    if (app_data->pixbuf) {
        int width = gdk_pixbuf_get_width(app_data->pixbuf);
        int height = gdk_pixbuf_get_height(app_data->pixbuf);

        if (ix >= 0 && ix < width && iy >= 0 && iy < height) {
            guchar r, g, b;
            get_pixel_color(app_data->pixbuf, ix, iy, &r, &g, &b);
            
            g_print("Coordinates (%d, %d) - RGB: (%d, %d, %d)\n", ix, iy, r, g, b);
            show_color_window(app_data, r, g, b);
        }
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    
    app_data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(app_data->window), "Image Pixel Color Picker");
    
    // Load the image (replace "image.png" with your image filename)
    app_data->pixbuf = gdk_pixbuf_new_from_file("Lucky.png", NULL);
    if (app_data->pixbuf == NULL) {
        g_print("Failed to load image\n");
        return;
    }
    
    // Create image widget
    app_data->image = gtk_picture_new_for_pixbuf(app_data->pixbuf);
    gtk_window_set_child(GTK_WINDOW(app_data->window), app_data->image);
    
    // Set up click gesture
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    g_signal_connect(click, "pressed", G_CALLBACK(on_image_click), app_data);
    gtk_widget_add_controller(app_data->image, GTK_EVENT_CONTROLLER(click));
    
    gtk_widget_show(app_data->window);
}

int main(int argc, char *argv[]) {
    AppData app_data = {NULL, NULL, NULL, NULL};
    
    GtkApplication *app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &app_data);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    // Cleanup
    if (app_data.pixbuf) {
        g_object_unref(app_data.pixbuf);
    }
    g_object_unref(app);
    
    return status;
}