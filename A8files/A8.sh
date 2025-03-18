# Compile the GTK4 audio application with PulseAudio support
gcc `pkg-config --cflags gtk4 libpulse-simple` -o A8 A8.c `pkg-config --libs gtk4 libpulse-simple` -lm

# Run the application
./A8 10