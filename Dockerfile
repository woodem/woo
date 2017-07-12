FROM ubuntu:16.04
RUN apt update && apt install -y wget python3
RUN python3 scripts/woo-install.py -j2 --headless --clean

