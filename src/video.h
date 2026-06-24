/*
 * video.h — Captura V4L2 via mmap, formato NV12
 */
#pragma once

#include "common.h"

#define VIDEO_BUF_COUNT  2   /* mínimo de buffers = menor latência de fila */

typedef struct {
    void   *start;
    size_t  length;
} VideoBuffer;

/* Callback chamado após VIDIOC_STREAMON — momento certo para aplicar controles */
typedef void (*VideoOnStreamReady) (void *userdata);

typedef struct {
    int                 fd;
    VideoBuffer         bufs[VIDEO_BUF_COUNT];
    int                 width;
    int                 height;
    int                 fps;
    char               *device;
    VideoOnStreamReady  on_stream_ready;
    void               *on_stream_ready_data;
} VideoDevice;

/* Frame dequeued do V4L2 — passado pro caller sem cópia */
typedef struct {
    const uint8_t *y_plane;   /* plano Y  (width × height bytes)     */
    const uint8_t *uv_plane;  /* plano UV (width × height/2 bytes)   */
    uint32_t       index;     /* índice do buffer V4L2 interno        */
} VideoFrame;

/*
 * Abre o device, negocia NV12, mapeia buffers mmap, inicia stream.
 * Atualiza dev->width/height com os valores ajustados pelo driver.
 */
bool video_open  (VideoDevice *dev);

/* Para stream, desmapeia buffers, fecha fd. Seguro chamar se open falhou. */
void video_close (VideoDevice *dev);

void video_set_on_stream_ready (VideoDevice *dev, VideoOnStreamReady cb, void *userdata);

/*
 * Dequeues o próximo frame disponível.
 * Retorna false se não há frame pronto (EAGAIN) ou em erro fatal.
 * Em erro fatal seta *fatal = true.
 */
bool video_dequeue (VideoDevice *dev, VideoFrame *frame, bool *fatal);

/* Re-enfileira o buffer após o caller ter terminado de usar o frame. */
void video_enqueue (VideoDevice *dev, uint32_t index);
