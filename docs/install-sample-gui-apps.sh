#!/bin/bash

# Make sure you are using Ubuntu 20.04!!

apt update

## Gedit

apt install gedit -y

## GIMP

apt install gimp -y

## Nautilus

apt install nautilus -y

## X11 apps

apt install x11-apps -y

## Google Chrome

cd /tmp
wget https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb
dpkg -i google-chrome-stable_current_amd64.deb 
apt install --fix-broken -y
dpkg -i google-chrome-stable_current_amd64.deb

## Microsoft teams

cd /tmp
curl -L -o "./teams.deb" "https://teams.microsoft.com/downloads/desktopurl?env=production&plat=linux&arch=x64&download=true&linuxArchiveType=deb"
apt install ./teams.deb -y

## Edge Browser

curl https://packages.microsoft.com/repos/edge/pool/main/m/microsoft-edge-dev/microsoft-edge-dev_88.0.673.0-1_amd64.deb -o /tmp/edge.deb
apt install /tmp/edge.deb -y