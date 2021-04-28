FROM fedora:34

RUN dnf update -y \
    && dnf install -y uncrustify git nodejs clang-analyzer 'dnf-command(builddep)' \
    && dnf builddep -y epiphany \
    && dnf clean all
RUN npm install -g eslint

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
