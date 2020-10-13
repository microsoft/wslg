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