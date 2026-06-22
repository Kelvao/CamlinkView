/*
 * audio.h — Captura PipeWire → playback SDL2
 *
 * Captura áudio de um source PipeWire/PulseAudio pelo nome e roteia
 * para o dispositivo de saída padrão do SDL2.
 * Registrado com media.role=Production para que o Discord identifique
 * o stream e possa capturá-lo ao compartilhar a janela.
 */
#pragma once

#include "common.h"

#define AUDIO_RATE     48000
#define AUDIO_CHANNELS 2
#define AUDIO_QUANTUM  256   /* quantum PipeWire — menor = menor latência */

/*
 * Cria dois streams PipeWire nativos:
 *   - capture: lê do source da Camlink
 *   - playback: Stream/Output/Audio visível pro Discord ao compartilhar janela
 * Não usa SDL para áudio — bridge totalmente dentro do grafo PipeWire.
 */

typedef struct AudioState AudioState;

/*
 * Inicializa PipeWire capture + SDL2 playback.
 * source: nome do node PipeWire (ex: alsa_input.usb-Elgato...).
 * Retorna NULL em falha — não-fatal, caller continua sem áudio.
 */
AudioState *audio_create  (const char *source);

/* Para e libera todos os recursos. Seguro com NULL. */
void        audio_destroy (AudioState *audio);
