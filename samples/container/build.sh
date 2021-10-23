#!/bin/bash

sudo docker build -t xclock -f Dockerfile.xclock .
sudo docker build -t glxinfo -f Dockerfile.glxinfo .
sudo docker build -t glxgears -f Dockerfile.glxgears .