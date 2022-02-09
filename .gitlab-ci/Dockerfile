FROM alpine:latest

RUN apk update && apk add uncrustify bash python3 nodejs npm
RUN npm install -g eslint

ARG HOST_USER_ID=guest
ENV HOST_USER_ID ${HOST_USER_ID}

USER guest
WORKDIR /home/user

ENV LANG C.UTF-8
