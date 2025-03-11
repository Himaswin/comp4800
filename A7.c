#include <gtk/gtk.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 10  // Size of the circular buffer

typedef struct {
    AVFrame *frame;
    int filled;  
} FrameBuffer;

typedef struct {
    FrameBuffer buffer[BUFFER_SIZE];
    int write_index;
    int read_index;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    int terminate;
    float frame_rate;
    char *filename;
} ThreadData;

static ThreadData thread_data;
static GtkWidget *frame_display;
static guint timer_id = 0;

// Add a frame to the buffer
static void add_frame_to_buffer(ThreadData *data, AVFrame *frame) {
    pthread_mutex_lock(&data->mutex);
    
    // Wait until there's space in the buffer
    while (data->count == BUFFER_SIZE && !data->terminate) {
        pthread_cond_wait(&data->not_full, &data->mutex);
    }
    
    if (data->terminate) {
        pthread_mutex_unlock(&data->mutex);
        return;
    }
    
    // Add the frame to the buffer
    if (data->buffer[data->write_index].frame != NULL) {
        av_frame_free(&data->buffer[data->write_index].frame);
    }
    
    data->buffer[data->write_index].frame = av_frame_clone(frame);
    data->buffer[data->write_index].filled = 1;
    data->write_index = (data->write_index + 1) % BUFFER_SIZE;
    data->count++;
    
    pthread_cond_signal(&data->not_empty);
    pthread_mutex_unlock(&data->mutex);
}

// Get a frame from the buffer
static AVFrame *get_frame_from_buffer(ThreadData *data) {
    AVFrame *frame = NULL;
    
    pthread_mutex_lock(&data->mutex);
    
    // Wait until there's a frame in the buffer
    while (data->count == 0 && !data->terminate) {
        pthread_cond_wait(&data->not_empty, &data->mutex);
    }
    
    if (data->terminate) {
        pthread_mutex_unlock(&data->mutex);
        return NULL;
    }
    
    // Get the frame from the buffer
    if (data->buffer[data->read_index].filled) {
        frame = data->buffer[data->read_index].frame;
        data->buffer[data->read_index].frame = NULL;
        data->buffer[data->read_index].filled = 0;
        data->read_index = (data->read_index + 1) % BUFFER_SIZE;
        data->count--;
        
        pthread_cond_signal(&data->not_full);
    }
    
    pthread_mutex_unlock(&data->mutex);
    return frame;
}

// Decoding thread function
static void *decode_thread(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVPacket *packet = NULL;
    AVFrame *frame = NULL;
    AVFrame *rgb_frame = NULL;
    struct SwsContext *sws_ctx = NULL;
    int video_stream_index = -1;
    
    // Open input file
    if (avformat_open_input(&fmt_ctx, data->filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", data->filename);
        goto cleanup;
    }
    
    // Retrieve stream information
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto cleanup;
    }
    
    // Find video stream
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    
    if (video_stream_index == -1) {
        fprintf(stderr, "Could not find video stream\n");
        goto cleanup;
    }
    
    // Get codec context
    AVCodecParameters *codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        goto cleanup;
    }
    
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate codec context\n");
        goto cleanup;
    }
    
    if (avcodec_parameters_to_context(codec_ctx, codecpar) < 0) {
        fprintf(stderr, "Could not copy codec parameters to context\n");
        goto cleanup;
    }
    
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        goto cleanup;
    }
    
    // Allocate frame and packet
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    
    if (!packet || !frame || !rgb_frame) {
        fprintf(stderr, "Could not allocate frames or packet\n");
        goto cleanup;
    }
    
    // Prepare RGB frame
    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = codecpar->width;
    rgb_frame->height = codecpar->height;
    
    if (av_frame_get_buffer(rgb_frame, 0) < 0) {
        fprintf(stderr, "Could not allocate RGB frame data\n");
        goto cleanup;
    }
    
    // Read frames and send them to the buffer
    while (av_read_frame(fmt_ctx, packet) >= 0 && !data->terminate) {
        if (packet->stream_index == video_stream_index) {
            int ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet for decoding\n");
                break;
            }
            
            while (ret >= 0 && !data->terminate) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fprintf(stderr, "Error receiving frame\n");
                    goto cleanup;
                }
                
                // Convert to RGB24
                if (!sws_ctx) {
                    sws_ctx = sws_getContext(
                        frame->width, frame->height, (enum AVPixelFormat)frame->format,
                        frame->width, frame->height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, NULL, NULL, NULL);
                    
                    if (!sws_ctx) {
                        fprintf(stderr, "Could not initialize the conversion context\n");
                        goto cleanup;
                    }
                }
                
                sws_scale(sws_ctx, (const uint8_t * const*)frame->data, frame->linesize,
                         0, frame->height, rgb_frame->data, rgb_frame->linesize);
                
                // Add frame to buffer
                add_frame_to_buffer(data, rgb_frame);
                
                // Slow down the decoding to match the desired frame rate
                usleep((1.0 / data->frame_rate) * 1000000);
            }
        }
        av_packet_unref(packet);
    }
    
cleanup:
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    sws_freeContext(sws_ctx);
    
    return NULL;
}

// Timer function for updating the display
static gboolean update_display(gpointer user_data) {
    ThreadData *data = (ThreadData *)user_data;
    
    AVFrame *frame = get_frame_from_buffer(data);
    if (frame) {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_data(
            frame->data[0],
            GDK_COLORSPACE_RGB,
            FALSE,
            8,
            frame->width,
            frame->height,
            frame->linesize[0],
            NULL,
            NULL);
            
        if (pixbuf) {
            gtk_picture_set_pixbuf(GTK_PICTURE(frame_display), pixbuf);
            g_object_unref(pixbuf);
        }
        av_frame_free(&frame);
    }
    
    // Check if we should continue
    if (data->terminate) {
        timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    
    return G_SOURCE_CONTINUE;
}

// Clean up resources
static void cleanup_resources() {
    if (timer_id > 0) {
        g_source_remove(timer_id);
        timer_id = 0;
    }
    
    pthread_mutex_lock(&thread_data.mutex);
    thread_data.terminate = 1;
    pthread_cond_signal(&thread_data.not_full);
    pthread_cond_signal(&thread_data.not_empty);
    pthread_mutex_unlock(&thread_data.mutex);
    
    // Clean up the frame buffer
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (thread_data.buffer[i].frame) {
            av_frame_free(&thread_data.buffer[i].frame);
        }
    }
    
    pthread_mutex_destroy(&thread_data.mutex);
    pthread_cond_destroy(&thread_data.not_full);
    pthread_cond_destroy(&thread_data.not_empty);
    
    free(thread_data.filename);
}

static void on_window_close(GtkWindow *window, gpointer user_data) {
    cleanup_resources();
}

static void activate(GtkApplication *app, gpointer user_data) {
    ThreadData *data = (ThreadData *)user_data;
    
    // Create a new window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Video Player");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    
    // Create the picture widget
    frame_display = gtk_picture_new();
    gtk_picture_set_can_shrink(GTK_PICTURE(frame_display), TRUE);
    gtk_picture_set_keep_aspect_ratio(GTK_PICTURE(frame_display), TRUE);
    
    // Create a box to hold the frame display
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), frame_display);
    
    // Set the box as the window's child
    gtk_window_set_child(GTK_WINDOW(window), box);
    
    // Connect window close signal
    g_signal_connect(window, "close-request", G_CALLBACK(on_window_close), NULL);
    
    // Start the decoding thread
    pthread_t decode_thread_id;
    if (pthread_create(&decode_thread_id, NULL, decode_thread, data) != 0) {
        fprintf(stderr, "Failed to create decoding thread\n");
        return;
    }
    pthread_detach(decode_thread_id);
    
    // Start the display timer
    timer_id = g_timeout_add((guint)(1000.0 / data->frame_rate), update_display, data);
    
    // Show the window
    gtk_widget_show(window);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <video_file> <frame_rate>\n", argv[0]);
        return 1;
    }
    
    // Parse frame rate
    float frame_rate = atof(argv[2]);
    if (frame_rate <= 0) {
        fprintf(stderr, "Invalid frame rate. Must be a positive number.\n");
        return 1;
    }
    
    // Initialize thread data
    memset(&thread_data, 0, sizeof(ThreadData));
    thread_data.frame_rate = frame_rate;
    thread_data.filename = strdup(argv[1]);
    
    // Initialize mutex and condition variables
    pthread_mutex_init(&thread_data.mutex, NULL);
    pthread_cond_init(&thread_data.not_full, NULL);
    pthread_cond_init(&thread_data.not_empty, NULL);
    
    // Create and run the application
    GtkApplication *app = gtk_application_new("com.example.videoplayer", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &thread_data);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);
    
    return status;
}
