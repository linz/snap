#

all:
	cd linux && make

test:
	cd linux && make test

snap_cmd:
	cd linux && make snap_cmd
