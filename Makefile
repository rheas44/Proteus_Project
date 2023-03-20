FEHSD_DEVICE=/dev/sdb1
TARGET=Proteus
export TARGET

all: build deploy

build:
	$(MAKE) -C fehproteusfirmware all

clean:
	$(MAKE) -C fehproteusfirmware clean

ifeq ($(OS),Windows_NT)
deploy: build
	$(MAKE) -C fehproteusfirmware deploy
else
UNAME_S := $(shell uname -s)
deploy: build
ifeq ($(UNAME_S),Darwin)
	$(MAKE) -C fehproteusfirmware deploy
else
	sudo mkdir -p /media/FEHSD
	sudo mount $(FEHSD_DEVICE) /media/FEHSD
	sudo cp *.s19 /media/FEHSD/CODE.S19
	sudo umount $(FEHSD_DEVICE)
endif
endif