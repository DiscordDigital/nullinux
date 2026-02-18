FROM ubuntu:noble

COPY . /app
WORKDIR /app
RUN mkdir /export
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get --assume-yes --no-install-recommends install tzdata curl coreutils iproute2 psmisc kbd kmod nano iputils-ping fdisk gdisk libmagic-mgc cpio wget grub-pc-bin grub2-common sudo git make gcc bc xz-utils

ENTRYPOINT ["/bin/bash", "build.sh"]
CMD ["-q", "-s", "-x", "/export/"]
