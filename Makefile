CC=gcc
all:
	$(CC) -o final final.c -lavdevice -lavformat -lavcodec -lavutil
