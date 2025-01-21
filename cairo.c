#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h>

#define NUM_PARTICLES 10

typedef struct {
    gdouble x;
    gdouble y;
    gdouble dx;
    gdouble dy;
    gdouble radius;
    gdouble red;
    gdouble green;
    gdouble blue;
    gdouble color_change;
} Particle;

typedef struct {
    Particle particles[NUM_PARTICLES];
    gdouble rotation;
    GtkWidget *drawing_area;
} AnimationData;

static AnimationData animation;

static void init_particles() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        animation.particles[i].x = g_random_double_range(50, 350);
        animation.particles[i].y = g_random_double_range(50, 250);
        animation.particles[i].dx = g_random_double_range(-5, 5);
        animation.particles[i].dy = g_random_double_range(-5, 5);
        animation.particles[i].radius = g_random_double_range(10, 30);
        animation.particles[i].red = g_random_double();
        animation.particles[i].green = g_random_double();
        animation.particles[i].blue = g_random_double();
        animation.particles[i].color_change = g_random_double_range(0.01, 0.03);
    }
    animation.rotation = 0.0;
}

static void draw_star(cairo_t *cr, gdouble x, gdouble y, gdouble r, 
                     gdouble rotation, gdouble red, gdouble green, gdouble blue) {
    int points = 5;
    gdouble angle = rotation;
    gdouble inner_radius = r * 0.4;

    cairo_set_source_rgb(cr, red, green, blue);
    cairo_move_to(cr, x + r * cos(angle), y + r * sin(angle));

    for (int i = 0; i < points * 2; i++) {
        angle += M_PI / points;
        gdouble radius = (i % 2 == 0) ? inner_radius : r;
        cairo_line_to(cr, x + radius * cos(angle), 
                         y + radius * sin(angle));
    }

    cairo_close_path(cr);
    cairo_fill(cr);
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    // Clear background
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    // Draw particles
    for (int i = 0; i < NUM_PARTICLES; i++) {
        Particle *p = &animation.particles[i];
        draw_star(cr, p->x, p->y, p->radius, 
                 animation.rotation, p->red, p->green, p->blue);
    }

    return FALSE;
}

static void update_particle_colors() {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        Particle *p = &animation.particles[i];
        p->red += p->color_change;
        p->green += p->color_change;
        p->blue += p->color_change;

        if (p->red > 1.0 || p->red < 0.0) p->color_change = -p->color_change;
        
        p->red = CLAMP(p->red, 0.0, 1.0);
        p->green = CLAMP(p->green, 0.0, 1.0);
        p->blue = CLAMP(p->blue, 0.0, 1.0);
    }
}

static gboolean update_animation(gpointer data) {
    GtkWidget *drawing_area = GTK_WIDGET(data);
    int width, height;

    width = gtk_widget_get_allocated_width(drawing_area);
    height = gtk_widget_get_allocated_height(drawing_area);

    // Update rotation
    animation.rotation += 0.05;

    // Update particles
    for (int i = 0; i < NUM_PARTICLES; i++) {
        Particle *p = &animation.particles[i];
        
        // Update position
        p->x += p->dx;
        p->y += p->dy;

        // Bounce off walls
        if (p->x - p->radius <= 0 || p->x + p->radius >= width) {
            p->dx = -p->dx;
        }
        if (p->y - p->radius <= 0 || p->y + p->radius >= height) {
            p->dy = -p->dy;
        }
    }

    // Update colors
    update_particle_colors();

    // Request redraw
    gtk_widget_queue_draw(drawing_area);

    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *drawing_area;

    // Initialize particles
    init_particles();

    // Create window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "A2 demo");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Create drawing area
    drawing_area = gtk_drawing_area_new();
    animation.drawing_area = drawing_area;
    gtk_container_add(GTK_CONTAINER(window), drawing_area);

    // Connect signals
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(on_draw), NULL);

    // Start animation timer
    g_timeout_add(16, update_animation, drawing_area);

    gtk_widget_show_all(window);
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