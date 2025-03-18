#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>  // For gboolean, TRUE, FALSE

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024
#define PI 3.14159265358979323846

typedef struct {
    pa_simple *pa_stream;
    double phase;
    double frequency;
} AppData;

static float generate_sample(AppData *app_data, double phase) {
    return sin(phase);
}

static void print_device_info() {
    // This function uses pactl to print audio device information
    printf("Device ID, Volume, and Sample Rate info:\n");
    system("pactl list sinks | grep -E 'Name|Volume|Sample'");  // Get relevant information about the default audio device
}

static gboolean generate_audio(AppData *app_data) {
    if (!app_data->pa_stream) {
        return FALSE;
    }

    // Generate audio buffer
    float buffer[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++) {
        app_data->phase += 2.0 * PI * app_data->frequency / SAMPLE_RATE;
        if (app_data->phase > 2.0 * PI)
            app_data->phase -= 2.0 * PI;

        buffer[i] = generate_sample(app_data, app_data->phase);
    }

    // Write samples to audio stream
    int error;
    if (pa_simple_write(app_data->pa_stream, buffer, sizeof(buffer), &error) < 0) {
        fprintf(stderr, "PulseAudio write failed: %s\n", pa_strerror(error));
        return FALSE;
    }

    return TRUE;
}

int main(int argc, char **argv) {
    AppData app_data = {0};
    int duration = 5;  // Default duration in seconds
    app_data.frequency = 440.0;  // A4 note (440Hz)

    if (argc > 1) {
        duration = atoi(argv[1]);  // Duration passed as command-line argument
    }

    // Print device info
    print_device_info();

    // PulseAudio settings
    static const pa_sample_spec sample_spec = {
        .format = PA_SAMPLE_FLOAT32LE,
        .rate = SAMPLE_RATE,
        .channels = 1
    };

    int error;
    app_data.pa_stream = pa_simple_new(NULL, "AudioTest", PA_STREAM_PLAYBACK, NULL, "playback", &sample_spec, NULL, NULL, &error);
    if (!app_data.pa_stream) {
        fprintf(stderr, "PulseAudio initialization failed: %s\n", pa_strerror(error));
        return 1;
    }

    printf("Starting playback for %d seconds...\n", duration);

    // Generate audio for the specified duration
    for (int i = 0; i < duration * SAMPLE_RATE / BUFFER_SIZE; i++) {
        if (!generate_audio(&app_data)) {
            break;
        }
    }

    printf("Stopping playback...\n");

    // Clean up
    pa_simple_free(app_data.pa_stream);

    return 0;
}
