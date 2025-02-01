#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *image;
    GtkWidget *box;
    FILE *file;
    const char *filename = "Lucky.png";

    // Check if the file exists
    file = fopen(filename, "rb");
    if (file == NULL) {
        g_printerr("Error: Cannot open file %s\n", filename);
        return;
    }
    fclose(file);

    // Create a new window
    window = gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(window), "Image Viewer");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400); // Set initial size

    // Create a GtkBox container
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), box);

    // Create an image widget and load the PNG file
    image = gtk_image_new_from_file(filename);

    // Add the image to the box and make it expand to fill the space
    gtk_box_append(GTK_BOX(box), image);
    gtk_widget_set_hexpand(image, TRUE);
    gtk_widget_set_vexpand(image, TRUE);

    // Show the window with the image
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.example.imageviewer", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}