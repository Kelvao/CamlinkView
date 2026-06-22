/*
 * detect.h — Auto-detecção de dispositivos Elgato Camlink 4K
 *
 * Varre /dev/video* via v4l2 e PipeWire/PulseAudio sources procurando
 * por dispositivos com "Cam Link" ou "Elgato" no nome.
 */
#pragma once

#include "common.h"

/*
 * Resultado da detecção automática.
 * Strings apontam para buffers internos estáticos — não liberar.
 */
typedef struct {
    const char *video_device;  /* ex: "/dev/video0", NULL se não encontrado */
    const char *audio_source;  /* ex: "alsa_input.usb-Elgato...", NULL se não encontrado */
} DetectResult;

/*
 * Detecta video e audio da Camlink automaticamente.
 * Retorna DetectResult com NULLs nos campos não encontrados.
 */
DetectResult detect_camlink (void);

/* Imprime os dispositivos encontrados de forma amigável. */
void detect_print (const DetectResult *r);
