# sboy - SMA SunnyBoy inverter reader
# Copyright (C) 2012 R.J.Middleton
# e-mail: dick@lingbrae.com
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

############################################################################
#                                    
# File Name ......................... Makefile
# Written By ........................ Dick Middleton
# Date .............................. 06-Nov-12
#
# Last edit: "Sboy 2012-11-14 2048"; /* auto updated by emacs */
#
# Description :  Makefile
#
# For development set CDEV
# export CDEV='-g -pedantic -std=c99 -Wall -DENABLE_ASSERT'
#
#
############################################################################
CC = gcc
CFLAGS = -DHAVE_CONFIG_H -D_XOPEN_SOURCE ${CDEV}
INCLUDES =  
LDLIBS = -lbluetooth
SRC = sboy.c sboy.ggo config.h Makefile LICENSE README
COBS = sboy.o cmdline.o 
CMD = cmdline.c cmdline.h
BIN = sboy sboy-test

all:    sboy

$(BIN):	$(COBS)
	$(CC) $(COBS) $(LDLIBS)  -o $@


$(COBS):	cmdline.h config.h

$(CMD):	sboy.ggo
	gengetopt -i $< 

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(COBS) $(BIN) *~

#..................................................

scrubup:
	rm -f $(COBS) $(CMD) $(BIN) *~

txfr:	sboy
	rsync -av $(SRC) $(CMD) dick@hagrid:sboy/

# Local Variables:
# mode: Makefile
# time-stamp-pattern: "30/Sboy[ \t]+%:y-%02m-%02d %02H%02M"
# End:
#                           ===//===
