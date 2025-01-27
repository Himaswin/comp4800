#include <gtk/gtk.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <cairo.h>

struct Point {
    double x, y;
    int cluster = -1; // Cluster assignment
};

struct Centroid {
    double x, y;
};

std::vector<Point> points;
std::vector<Centroid> centroids;
std::atomic<bool> proceed(false); // Indicates when to proceed to the next iteration
std::atomic<int> current_iteration(1); // Current iteration counter

// Function to read data from file
void read_from_file(const std::string& file_name) {
    std::ifstream file(file_name);
    if (!file) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    int num_points, num_centroids;
    file >> num_points;
    points.resize(num_points);

    for (int i = 0; i < num_points; ++i) {
        file >> points[i].x >> points[i].y;
    }

    file >> num_centroids;
    centroids.resize(num_centroids);

    for (int i = 0; i < num_centroids; ++i) {
        file >> centroids[i].x >> centroids[i].y;
    }
}

static gboolean on_key_press(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    if (keyval == GDK_KEY_Return) { // "Enter" key
        proceed = true; // Allow the next iteration to proceed
        return TRUE;
    }
    return FALSE;
}

// Function to calculate distance between two points
double calculate_distance(const Point& p, const Centroid& c) {
    return std::sqrt(std::pow(p.x - c.x, 2) + std::pow(p.y - c.y, 2));
}

// Perform one iteration of K-Means
bool kmeans_iteration() {
    bool changed = false;

    // Assign points to the nearest centroid
    for (auto& point : points) {
        int closest_cluster = -1;
        double min_distance = std::numeric_limits<double>::max();

        for (size_t i = 0; i < centroids.size(); ++i) {
            double distance = calculate_distance(point, centroids[i]);
            if (distance < min_distance) {
                min_distance = distance;
                closest_cluster = i;
            }
        }

        if (point.cluster != closest_cluster) {
            point.cluster = closest_cluster;
            changed = true;
        }
    }

    // Recalculate centroids
    std::vector<int> cluster_sizes(centroids.size(), 0);
    std::vector<Centroid> new_centroids(centroids.size(), {0, 0});

    for (const auto& point : points) {
        if (point.cluster != -1) {
            new_centroids[point.cluster].x += point.x;
            new_centroids[point.cluster].y += point.y;
            cluster_sizes[point.cluster]++;
        }
    }

    for (size_t i = 0; i < centroids.size(); ++i) {
        if (cluster_sizes[i] > 0) {
            centroids[i].x = new_centroids[i].x / cluster_sizes[i];
            centroids[i].y = new_centroids[i].y / cluster_sizes[i];
        }
    }

    return changed;
}

static void on_draw(GtkDrawingArea* drawing_area, cairo_t* cr, int width, int height, gpointer user_data) {
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    const double scale = 20.0;
    const double x_offset = width / 2;
    const double y_offset = height / 2;

    // Draw X and Y axes
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 2);

    // X axis
    cairo_move_to(cr, 0, height / 2);
    cairo_line_to(cr, width, height / 2);
    cairo_stroke(cr);

    // Y axis
    cairo_move_to(cr, width / 2, 0);
    cairo_line_to(cr, width / 2, height);
    cairo_stroke(cr);

    // Draw axis labels and values
    cairo_set_font_size(cr, 10);
    
    // X axis values
    for (int i = -20; i <= 20; i++) {
        double x = x_offset + i * scale;
        
        // Draw tick marks
        cairo_move_to(cr, x, y_offset - 3);
        cairo_line_to(cr, x, y_offset + 3);
        cairo_stroke(cr);

        // Draw numbers
        cairo_text_extents_t extents;
        std::string text = std::to_string(i);
        cairo_text_extents(cr, text.c_str(), &extents);
        cairo_move_to(cr, x - extents.width/2, y_offset + 15);
        cairo_show_text(cr, text.c_str());
    }

    // Y axis values
    for (int i = -15; i <= 15; i++) {
        double y = y_offset - i * scale;
        
        // Draw tick marks
        cairo_move_to(cr, x_offset - 3, y);
        cairo_line_to(cr, x_offset + 3, y);
        cairo_stroke(cr);

        if (i != 0) {
            std::string text = std::to_string(i);
            cairo_text_extents_t extents;
            cairo_text_extents(cr, text.c_str(), &extents);
            cairo_move_to(cr, x_offset - extents.width - 5, y + extents.height/2);
            cairo_show_text(cr, text.c_str());
        }
    }

    // Draw grid lines
    cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.5);
    cairo_set_line_width(cr, 0.5);

    // Vertical grid lines
    for (int i = -20; i <= 20; i++) {
        if (i == 0) continue;
        double x = x_offset + i * scale;
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
        cairo_stroke(cr);
    }

    // Horizontal grid lines
    for (int i = -15; i <= 15; i++) {
        if (i == 0) continue;
        double y = y_offset - i * scale;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
        cairo_stroke(cr);
    }

    // Display iteration number and instructions
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_font_size(cr, 16);
    std::string iter_text = "Iteration: " + std::to_string(current_iteration);
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, iter_text.c_str());

    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, 20, 50);
    cairo_show_text(cr, "Press Enter to proceed to next iteration");

    // Draw points
    for (const auto& point : points) {
        if (point.cluster == -1) {
            cairo_set_source_rgb(cr, 1, 0, 0); // Red for unassigned points
        } else {
            switch (point.cluster) {
                case 0: cairo_set_source_rgb(cr, 0, 1, 0); break; // Green for Cluster 1
                case 1: cairo_set_source_rgb(cr, 0, 0, 1); break; // Blue for Cluster 2
                case 2: cairo_set_source_rgb(cr, 1, 0.5, 0); break; // Orange for Cluster 3
                default: cairo_set_source_rgb(cr, 0, 0, 0); break; // Black for undefined
            }
        }
        cairo_arc(cr, x_offset + point.x * scale, y_offset - point.y * scale, 5, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Draw centroids as triangles
    for (const auto& centroid : centroids) {
        cairo_set_source_rgb(cr, 0, 0, 0); // Black for centroids
        double cx = x_offset + centroid.x * scale;
        double cy = y_offset - centroid.y * scale;

        cairo_move_to(cr, cx, cy - 10);
        cairo_line_to(cr, cx - 7, cy + 7);
        cairo_line_to(cr, cx + 7, cy + 7);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
}

void run_kmeans(GtkWidget* drawing_area) {
    while (true) {
        gtk_widget_queue_draw(drawing_area);

        proceed = false;
        while (!proceed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!kmeans_iteration()) {
            break;
        }

        current_iteration++;
    }

    gtk_widget_queue_draw(drawing_area);
}

void on_activate(GtkApplication* app, gpointer user_data) {
    current_iteration = 1;
    read_from_file("data3.txt");

    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "K-Means Visualization");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    GtkWidget* drawing_area = gtk_drawing_area_new();
    gtk_window_set_child(GTK_WINDOW(window), drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), on_draw, nullptr, nullptr);

    GtkEventController* key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_key_press), nullptr);
    gtk_widget_add_controller(window, key_controller);

    gtk_window_present(GTK_WINDOW(window));

    std::thread kmeans_thread(run_kmeans, drawing_area);
    kmeans_thread.detach();
}

int main(int argc, char* argv[]) {
    GtkApplication* app = gtk_application_new("org.example.KMeansApp", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}