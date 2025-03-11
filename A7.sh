gcc `pkg-config --cflags gtk4` -o A7 A7.c `pkg-config --libs gtk4` -lavformat -lavcodec -lswscale -lavutil -lpthread

./A7 sample.mp4 20
