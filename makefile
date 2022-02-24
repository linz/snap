#

all:
	make -C linux all

test:
	make -C linux test

install:
	make -C linux install

clean:
	make -C linux clean

snap_cmd:
	cd linux && make snap_cmd
