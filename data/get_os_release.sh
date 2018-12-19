#!/bin/sh
sed -n '/^NAME=/p' /etc/os-release | sed -r 's/.*=([a-zA-Z]+).*/\1/'
