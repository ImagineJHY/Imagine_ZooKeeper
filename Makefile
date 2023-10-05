.PHONY: build
all:
	cd build && make
init:
	cd build && make init
prepare:
	cd build && cmake .. && make prepare
build:
	cd build && cmake .. && make build
clean:
	cd build && make clean
