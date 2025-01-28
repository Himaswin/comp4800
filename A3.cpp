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
#include <iomanip>

// Forward declarations
void print_iteration(int iteration);
static void on_pause_clicked(GtkWidget* widget, gpointer data);
static void on_step_clicked(GtkWidget* widget, gpointer data);
static void on_back_clicked(GtkWidget* widget, gpointer data);
static void on_speed_changed(GtkRange* range, gpointer user_data);
static void on_restart_clicked(GtkWidget* widget, gpointer data);

struct Point {
    double x, y;
    int cluster = -1;
};

struct Centroid {
    double x, y;
};

struct Color {
    double r, g, b;
};

// Global variables
std::vector<Point> points;
std::vector<Centroid> centroids;
std::atomic<int> current_iteration(1);
std::atomic<bool> is_paused(false);
std::atomic<bool> step_requested(false);
std::vector<std::vector<Centroid>> centroid_history;
std::vector<std::vector<Point>> point_history;
std::atomic<int> iteration_speed(2000); // Default speed in milliseconds

Color get_distinct_color(int index, int total) {
    Color color;
    if (total <= 0) total = 1;
    
    double hue = fmod(index * (360.0 / total), 360.0);
    double saturation = 0.7 + (index % 3) * 0.1;
    double value = 0.8 + (index % 2) * 0.1;

    double h = hue / 60.0;
    double c = value * saturation;
    double x = c * (1 - fabs(fmod(h, 2.0) - 1));
    double m = value - c;

    if (h >= 0 && h < 1) {
        color.r = c; color.g = x; color.b = 0;
    } else if (h >= 1 && h < 2) {
        color.r = x; color.g = c; color.b = 0;
    } else if (h >= 2 && h < 3) {
        color.r = 0; color.g = c; color.b = x;
    } else if (h >= 3 && h < 4) {
        color.r = 0; color.g = x; color.b = c;
    } else if (h >= 4 && h < 5) {
        color.r = x; color.g = 0; color.b = c;
    } else {
        color.r = c; color.g = 0; color.b = x;
    }

    color.r += m;
    color.g += m;
    color.b += m;

    return color;
}

// Button callbacks
static void on_pause_clicked(GtkWidget* widget, gpointer data) {
    is_paused = !is_paused;
    const char* label = is_paused ? "Resume" : "Pause";
    gtk_button_set_label(GTK_BUTTON(widget), label);
}

static void on_step_clicked(GtkWidget* widget, gpointer data) {
    if (is_paused) {
        step_requested = true;
    }
}

static void on_back_clicked(GtkWidget* widget, gpointer data) {
    if (is_paused && current_iteration > 1) {
        current_iteration--;
        if (current_iteration - 1 < point_history.size()) {
            points = point_history[current_iteration - 1];
            centroids = centroid_history[current_iteration - 1];
            gtk_widget_queue_draw(GTK_WIDGET(data));
        }
    }
}

static void on_speed_changed(GtkRange* range, gpointer user_data) {
    iteration_speed = gtk_range_get_value(range);
}



void print_iteration(int iteration) {
    std::vector<int> cluster_sizes(centroids.size(), 0);

    for (const auto& point : points) {
        if (point.cluster != -1) {
            cluster_sizes[point.cluster]++;
        }
    }

    std::cout << "\n=== Iteration " << iteration << " ===\n";
    std::cout << "Number of centroids: " << centroids.size() << std::endl;
    
    for (size_t i = 0; i < centroids.size(); ++i) {
        std::cout << "Centroid " << i + 1 << ": ("
                  << std::fixed << std::setprecision(2) << centroids[i].x << ", " 
                  << centroids[i].y << ") -> Points in cluster: " 
                  << cluster_sizes[i] << "\n";
    }

    std::cout << "\nPoint Assignments:\n";
    for (size_t i = 0; i < points.size(); ++i) {
        std::cout << "Point " << i + 1 << ": ("
                  << std::fixed << std::setprecision(2) << points[i].x << ", " 
                  << points[i].y << ") -> Cluster " 
                  << points[i].cluster + 1 << "\n";
    }
    std::cout << std::endl;
}

void read_from_file(const std::string& file_name) {
    std::ifstream file(file_name);
    if (!file) {
        std::cerr << "Error opening file!" << std::endl;
        return;
    }

    points.clear();
    centroids.clear();
    centroid_history.clear();
    point_history.clear();

    int num_points;
    file >> num_points;
    
    for (int i = 0; i < num_points; ++i) {
        Point p;
        if (file >> p.x >> p.y) {
            p.cluster = -1;
            points.push_back(p);
        }
    }

    int num_centroids;
    file >> num_centroids;

    for (int i = 0; i < num_centroids; ++i) {
        Centroid c;
        if (file >> c.x >> c.y) {
            centroids.push_back(c);
        }
    }

    point_history.push_back(points);
    centroid_history.push_back(centroids);

    file.close();
}

double calculate_distance(const Point& p, const Centroid& c) {
    return std::sqrt(std::pow(p.x - c.x, 2) + std::pow(p.y - c.y, 2));
}

bool kmeans_iteration() {
    bool changed = false;
    const size_t num_centroids = centroids.size();

    point_history.push_back(points);
    centroid_history.push_back(centroids);

    for (auto& point : points) {
        int closest_cluster = -1;
        double min_distance = std::numeric_limits<double>::max();

        for (size_t i = 0; i < num_centroids; ++i) {
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

    std::vector<double> sum_x(num_centroids, 0.0);
    std::vector<double> sum_y(num_centroids, 0.0);
    std::vector<int> count(num_centroids, 0);

    for (const auto& point : points) {
        if (point.cluster >= 0 && point.cluster < num_centroids) {
            sum_x[point.cluster] += point.x;
            sum_y[point.cluster] += point.y;
            count[point.cluster]++;
        }
    }

    for (size_t i = 0; i < num_centroids; ++i) {
        if (count[i] > 0) {
            centroids[i].x = sum_x[i] / count[i];
            centroids[i].y = sum_y[i] / count[i];
        }
    }

    return changed;
}

static void on_draw(GtkDrawingArea* drawing_area, cairo_t* cr, int width, int height, gpointer user_data) {
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    const double range = 100.0;
    const double scale = std::min(width, height) / (2.0 * range);
    const double x_offset = width / 2;
    const double y_offset = height / 2;

    // Draw grid lines (very light)
    cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 0.5);
    cairo_set_line_width(cr, 0.5);

    int step = 10;
    
    // Grid lines
    for (int i = -range; i <= range; i += step) {
        if (i == 0) continue;
        double x = x_offset + i * scale;
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
        cairo_stroke(cr);

        double y = y_offset - i * scale;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
        cairo_stroke(cr);
    }

    // Draw axes (darker)
    cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, 0, height / 2);
    cairo_line_to(cr, width, height / 2);
    cairo_move_to(cr, width / 2, 0);
    cairo_line_to(cr, width / 2, height);
    cairo_stroke(cr);

    // Draw labels
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_font_size(cr, std::min(width, height) / 80.0);
    
    for (int i = -range; i <= range; i += step) {
        double x = x_offset + i * scale;
        double y = y_offset - i * scale;
        std::string text = std::to_string(i);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, text.c_str(), &extents);
        
        // X axis labels
        cairo_move_to(cr, x - extents.width/2, y_offset + extents.height + 5);
        cairo_show_text(cr, text.c_str());
        
        // Y axis labels (skip 0)
        if (i != 0) {
            cairo_move_to(cr, x_offset - extents.width - 5, y + extents.height/2);
            cairo_show_text(cr, text.c_str());
        }
    }

    // Display status information
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_set_font_size(cr, std::min(width, height) / 30.0);
    
    std::string status = is_paused ? " (Paused - Use Step/Back)" : " (Running)";
    std::string iter_text = "Iteration: " + std::to_string(current_iteration) + status;
    cairo_move_to(cr, 20, 30);
    cairo_show_text(cr, iter_text.c_str());

    std::string speed_text = "Speed: " + std::to_string(iteration_speed) + "ms";
    cairo_move_to(cr, width - 150, 30);
    cairo_show_text(cr, speed_text.c_str());

    cairo_set_font_size(cr, std::min(width, height) / 40.0);
    std::string info_text = "Points: " + std::to_string(points.size()) + 
                           "  Centroids: " + std::to_string(centroids.size());
    cairo_move_to(cr, 20, 60);
    cairo_show_text(cr, info_text.c_str());

    // Draw points
    double point_size = std::min(width, height) / 100.0;
    for (const auto& point : points) {
        if (point.cluster == -1) {
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        } else {
            Color color = get_distinct_color(point.cluster, centroids.size());
            cairo_set_source_rgb(cr, color.r, color.g, color.b);
        }
        cairo_arc(cr, x_offset + point.x * scale, y_offset - point.y * scale, point_size, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Draw centroids
    double centroid_size = std::min(width, height) / 60.0;
    for (size_t i = 0; i < centroids.size(); ++i) {
        const auto& centroid = centroids[i];
        Color color = get_distinct_color(i, centroids.size());
        cairo_set_source_rgb(cr, color.r * 0.7, color.g * 0.7, color.b * 0.7);
        
        double cx = x_offset + centroid.x * scale;
        double cy = y_offset - centroid.y * scale;

        cairo_move_to(cr, cx, cy - centroid_size);
        cairo_line_to(cr, cx - centroid_size, cy + centroid_size);
        cairo_line_to(cr, cx + centroid_size, cy + centroid_size);
        cairo_close_path(cr);
        cairo_fill(cr);
        
        double brightness = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
        cairo_set_source_rgb(cr, brightness < 0.5 ? 1 : 0, brightness < 0.5 ? 1 : 0, brightness < 0.5 ? 1 : 0);
        cairo_set_font_size(cr, centroid_size);
        std::string num = std::to_string(i + 1);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, num.c_str(), &extents);
        cairo_move_to(cr, cx - extents.width/2, cy + extents.height/2);
        cairo_show_text(cr, num.c_str());
    }
}

void run_kmeans(GtkWidget* drawing_area) {
    centroid_history.clear();
    point_history.clear();
    
    while (true) {
        gtk_widget_queue_draw(drawing_area);
        
        while (is_paused) {
            if (step_requested) {
                step_requested = false;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(iteration_speed));

        if (!kmeans_iteration()) {
            break;
        }

        current_iteration++;
    }

    gtk_widget_queue_draw(drawing_area);
    std::cout << "K-Means completed in " << current_iteration << " iterations." << std::endl;
}

void on_activate(GtkApplication* app, gpointer user_data) {
    current_iteration = 1;
    read_from_file("data3.txt");

    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "K-Means Visualization");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

    // Create a vertical box to hold everything
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    // Create drawing area
    GtkWidget* drawing_area = gtk_drawing_area_new();
    gtk_widget_set_vexpand(drawing_area, TRUE);
    gtk_box_append(GTK_BOX(vbox), drawing_area);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), on_draw, nullptr, nullptr);

    // Create horizontal box for controls
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(vbox), hbox);

    // Create buttons
    GtkWidget* pause_button = gtk_button_new_with_label("Pause");
    GtkWidget* step_button = gtk_button_new_with_label("forward");
    GtkWidget* back_button = gtk_button_new_with_label("Back");

    // Create speed control slider
    GtkWidget* speed_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget* speed_label = gtk_label_new("Speed (ms):");
    GtkWidget* speed_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 100, 3000, 100);
    gtk_range_set_value(GTK_RANGE(speed_slider), iteration_speed);
    gtk_scale_set_draw_value(GTK_SCALE(speed_slider), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(speed_slider), GTK_POS_RIGHT);
    
    // Add widgets to speed box
    gtk_box_append(GTK_BOX(speed_box), speed_label);
    gtk_box_append(GTK_BOX(speed_box), speed_slider);
    gtk_widget_set_size_request(speed_slider, 200, -1);

    // Add all controls to horizontal box
    gtk_box_append(GTK_BOX(hbox), pause_button);
    gtk_box_append(GTK_BOX(hbox), step_button);
    gtk_box_append(GTK_BOX(hbox), back_button);
    gtk_box_append(GTK_BOX(hbox), speed_box);

    // Set margins and spacing
    gtk_widget_set_margin_start(hbox, 5);
    gtk_widget_set_margin_end(hbox, 5);
    gtk_widget_set_margin_bottom(hbox, 5);
    gtk_widget_set_margin_start(speed_box, 20);

    // Connect signals
    g_signal_connect(pause_button, "clicked", G_CALLBACK(on_pause_clicked), nullptr);
    g_signal_connect(step_button, "clicked", G_CALLBACK(on_step_clicked), nullptr);
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_clicked), drawing_area);
    g_signal_connect(speed_slider, "value-changed", G_CALLBACK(on_speed_changed), nullptr);

    // Style adjustments
    gtk_widget_set_size_request(pause_button, 80, 30);
    gtk_widget_set_size_request(step_button, 80, 30);
    gtk_widget_set_size_request(back_button, 80, 30);

    gtk_window_present(GTK_WINDOW(window));

    // Run K-Means in a separate thread
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
