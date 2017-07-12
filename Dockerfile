FROM ubuntu:16.04
RUN apt update && apt install -y wget python3
# not sure where we actually are...?
RUN ls
# maybe this is necessary
WORKDIR ./woo
RUN python3 scripts/woo-install.py -j2 --headless --clean
