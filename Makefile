# This file is part of multicast_test.
#
# Copyright 2015 Ricardo Garcia <public@rg3.name>
#
# To the extent possible under law, the author(s) have dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty. 
#
# You should have received a copy of the CC0 Public Domain Dedication along
# with this software. If not, see
# <http://creativecommons.org/publicdomain/zero/1.0/>.
#
PROGS = multicast_send multicast_receive

.PHONY: all clean

all: $(PROGS)

clean:
	rm -f $(PROGS)
