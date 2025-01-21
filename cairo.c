#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>

#define NUM_STARS 8

typedef struct {
    gdouble x;
    gdouble y;
    gdouble dx;
    gdouble dy;
    gdouble size;
    gdouble rotation;
    gdouble rot_speed;
    gdouble red;
    gdouble green;
    gdouble blue;
    gdouble alpha;
} Star;

typedef struct {
    Star stars[NUM_STARS];
    GtkWidget *drawing_area;
} AnimationData;

static AnimationData animation = {0};

static void draw_star(cairo_t *cr, Star *star) {
    const int points = 5;
    const double inner_radius = star->size * 0.4;
    const double outer_radius = star->size;
    double angle = star->rotation;

    cairo_save(cr);
    cairo_translate(cr, star->x, star->y);

    // Add glow effect
    cairo_set_source_rgba(cr, star->red, star->green, star->blue, 0.2);
    cairo_arc(cr, 0, 0, outer_radius * 1.5, 0, 2 * M_PI);
    cairo_fill(cr);

    // Draw star
    cairo_move_to(cr, outer_radius * cos(angle), outer_radius * sin(angle));
    
    for (int i = 0; i < points * 2; i++) {
        angle += M_PI / points;
        double radius = (i % 2 == 0) ? inner_radius : outer_radius;
        cairo_line_to(cr, radius * cos(angle), radius * sin(angle));
    }

    cairo_close_path(cr);

    // Fill with gradient
    cairo_pattern_t *pattern = cairo_pattern_create_radial(
        0, 0, inner_radius,
        0, 0, outer_radius);
    cairo_pattern_add_color_stop_rgba(pattern, 0, 
        star->red, star->green, star->blue, 1.0);
    cairo_pattern_add_color_stop_rgba(pattern, 1, 
        star->red * 0.5, star->green * 0.5, star->blue * 0.5, 0.8);
    
    cairo_set_source(cr, pattern);
    cairo_fill_preserve(cr);
    
    // Add outline
    cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    cairo_pattern_destroy(pattern);
    cairo_restore(cr);
}

static void draw_function(GtkDrawingArea *area, cairo_t *cr, 
                         int width, int height, gpointer user_data) {
    // Draw dark background with gradient
    cairo_pattern_t *bg_pattern = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgb(bg_pattern, 0, 0.1, 0.1, 0.2);
    cairo_pattern_add_color_stop_rgb(bg_pattern, 1, 0.0, 0.0, 0.1);
    cairo_set_source(cr, bg_pattern);
    cairo_paint(cr);
    cairo_pattern_destroy(bg_pattern);

    // Draw stars
    for (int i = 0; i < NUM_STARS; i++) {
        draw_star(cr, &animation.stars[i]);
    }
}

static void update_star_color(Star *star) {
    // Smoothly transition colors
    double speed = 0.02;
    star->red += (g_random_double() - 0.5) * speed;
    star->green += (g_random_double() - 0.5) * speed;
    star->blue += (g_random_double() - 0.5) * speed;

    // Keep colors in valid range
    star->red = CLAMP(star->red, 0.2, 1.0);
    star->green = CLAMP(star->green, 0.2, 1.0);
    star->blue = CLAMP(star->blue, 0.2, 1.0);
}

static gboolean update_animation(GtkWidget *widget, GdkFrameClock *frame_clock, 
                               gpointer user_data) {
    int width = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);

    for (int i = 0; i < NUM_STARS; i++) {
        Star *star = &animation.stars[i];

        // Update position
        star->x += star->dx;
        star->y += star->dy;

        // Bounce off walls with color change
        if (star->x - star->size <= 0 || star->x + star->size >= width) {
            star->dx = -star->dx;
            update_star_color(star);
        }
        if (star->y - star->size <= 0 || star->y + star->size >= height) {
            star->dy = -star->dy;
            update_star_color(star);
        }

        // Rotate star
        star->rotation += star->rot_speed;
        if (star->rotation > 2 * M_PI) {
            star->rotation -= 2 * M_PI;
        }
    }

    gtk_widget_queue_draw(widget);
    return G_SOURCE_CONTINUE;
}

static void init_stars() {
    for (int i = 0; i < NUM_STARS; i++) {
        Star *star = &animation.stars[i];
        
        // Random position
        star->x = g_random_double_range(50, 350);
        star->y = g_random_double_range(50, 250);
        
        // Random velocity
        star->dx = g_random_double_range(-4, 4);
        star->dy = g_random_double_range(-4, 4);
        
        // Random size
        star->size = g_random_double_range(15, 30);
        
        // Random rotation
        star->rotation = g_random_double_range(0, 2 * M_PI);
        star->rot_speed = g_random_double_range(-0.05, 0.05);
        
        // Random color
        star->red = g_random_double_range(0.5, 1.0);
        star->green = g_random_double_range(0.5, 1.0);
        star->blue = g_random_double_range(0.5, 1.0);
        star->alpha = 1.0;
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *drawing_area;

    // Initialize stars
    init_stars();

    // Create window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Bouncing Stars");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Create drawing area
    drawing_area = gtk_drawing_area_new();
    animation.drawing_area = drawing_area;

    // Set up drawing area
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area),
                                  draw_function,
                                  NULL,
                                  NULL);

    // Add drawing area to window
    gtk_window_set_child(GTK_WINDOW(window), drawing_area);

    // Add tick callback for animation
    gtk_widget_add_tick_callback(drawing_area, update_animation, NULL, NULL);

    gtk_widget_show(window);
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
