#define _XOPEN_SOURCE 700
#include "detect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

/* ── Constantes ──────────────────────────────────────────────────────────── */

#define MAX_VIDEO_DEVICES  16
#define BUF_SIZE           256

/* Palavras-chave que identificam uma Camlink */
static const char * const VIDEO_KEYWORDS[] = {
    "Cam Link 4K", "CamLink", "cam link", "Elgato", NULL
};
static const char * const AUDIO_KEYWORDS[] = {
    "Cam_Link_4K", "CamLink", "cam_link", "Elgato", "Cam Link 4K Estéreo analógico", NULL
};

/* Buffers estáticos para os resultados */
static char s_video_buf[BUF_SIZE];
static char s_audio_buf[BUF_SIZE];

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static bool
str_contains_any (const char *haystack, const char * const *needles)
{
    if (!haystack || !needles) return false;

    for (int i = 0; needles[i]; i++) {
        // strcasestr busca a substring ignorando maiúsculas/minúsculas
        if (strcasestr(haystack, needles[i])) {
            return true;
        }
        
    }
    return false;
}

/* ── Detecção de vídeo ───────────────────────────────────────────────────── */

static const char *
detect_video (void)
{
    char path[32];
    struct v4l2_capability cap;

    for (int i = 0; i < MAX_VIDEO_DEVICES; i++) {
        snprintf (path, sizeof path, "/dev/video%d", i);

        int fd = open (path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        bool found = false;
        if (ioctl (fd, VIDIOC_QUERYCAP, &cap) == 0) {
            /* Checa card name e bus_info */
            found = str_contains_any ((const char *)cap.card,     VIDEO_KEYWORDS)
                 || str_contains_any ((const char *)cap.bus_info,  VIDEO_KEYWORDS);
        }
        close (fd);

        if (found) {
            snprintf (s_video_buf, sizeof s_video_buf, "%s", path);
            return s_video_buf;
        }
    }
    return NULL;
}

/* ── Detecção de áudio ───────────────────────────────────────────────────── */

static const char *
detect_audio (void)
{
    /*
     * Usa `pactl list sources short` — disponível em sistemas com
     * PulseAudio ou PipeWire-pulse. Formato de saída:
     *   <id>\t<name>\t<module>\t<format>\t<state>
     */
    FILE *f = popen ("pw-cli ls Node 2>/dev/null | grep 'node.name ='", "r");
    if (!f) return NULL;

    char line[512];
    const char *found = NULL;

    while (fgets (line, sizeof line, f)) {
        // Limpa a linha para pegar o conteúdo entre aspas
        char *desc = strchr (line, '"');
        if (!desc) continue;
        desc++; // Pula a primeira aspa
        
        char *end = strrchr (desc, '"');
        if (end) *end = '\0'; // Remove a última aspa

        if (str_contains_any (desc, AUDIO_KEYWORDS)) {
            snprintf (s_audio_buf, sizeof s_audio_buf, "%s", desc);
            found = s_audio_buf;
            break;
        }
    }

    pclose (f);
    return found;
}

/* ── API pública ─────────────────────────────────────────────────────────── */

DetectResult
detect_camlink (void)
{
    return (DetectResult) {
        .video_device = detect_video (),
        .audio_source = detect_audio (),
    };
}

void
detect_print (const DetectResult *r)
{
    printf ("  vídeo  : %s\n",
            r->video_device ? r->video_device : "(não encontrado)");
    printf ("  áudio  : %s\n",
            r->audio_source ? r->audio_source : "(não encontrado)");
}
