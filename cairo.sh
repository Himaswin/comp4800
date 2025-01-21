gcc -o cairo cairo.c `pkg-config --cflags --libs gtk+-3.0` -lm

./cairo