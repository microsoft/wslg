#!/bin/sh
mkdir -p /mnt/wsl/system-distro/
mkdir -p /mnt/wsl/system-distro/xdg-runtime-dir
mkdir -p /tmp/.X11-unix
chmod 777 /tmp/.X11-unix

# X / XWayland / Wayland
export XDG_RUNTIME_DIR=/mnt/wsl/system-distro/xdg-runtime-dir
export WAYLAND_DISPLAY=/mnt/wsl/system-distro/xdg-runtime-dir/wayland-0
export DISPLAY=:0

 # Xcursor
export XCURSOR_PATH=/usr/share/icons
export XCURSOR_THEME=whiteglass
export XCURSOR_SIZE=16

# PulseAudio
export PULSE_AUDIO_RDP_SINK=/mnt/wsl/system-distro/PulseAudioRDPSink
export PULSE_AUDIO_RDP_SOURCE=/mnt/wsl/system-distro/PulseAudioRDPSource

ldconfig

# pulse audio is disabled, since we need to figure how to launch it without root user
# Should we create a system-distro user?

# pulseaudio --load="module-rdp-sink sink_name=RDPSink" \
#           --load="module-rdp-source source_name=RDPSource" \
#           --load="module-native-protocol-unix socket=/mnt/wsl/system-distro/PulseServer auth-anonymous=true" &

weston --backend=rdp-backend.so --xwayland --port=3391 --shell=rdprail-shell.so