# camlink-view

Visualizador nativo GTK4 + GStreamer para Elgato Camlink 4K.

- Latência mínima via GStreamer direto no V4L2
- Áudio registrado como aplicação PipeWire — capturável pelo Discord ao compartilhar janela
- Fullscreen, zero dependências de runtime além de GTK4/GStreamer

## Build

```bash
./build.sh
```

## Uso

```bash
./builddir/camlink-view \
  --device=/dev/video0 \
  --audio=alsa_input.usb-Elgato_Cam_Link_4K_00051119F6000-03.analog-stereo \
  --format=yuyv422 \
  --resolution=1920x1080 \
  --fps=60
```

## Teclas

| Tecla | Ação |
|-------|------|
| F | Fullscreen |
| Q / Esc | Sair |

## Discord

Compartilhar janela → seleciona "Camlink View" → áudio vai junto automaticamente via PipeWire.
