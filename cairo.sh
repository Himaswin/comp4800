gcc -o cairo cairo.c `pkg-config --cflags --libs gtk4` -lm

./cairo
