/*
  Sboy - SMA SunnyBoy inverter reader
  Copyright (C) 2012 R.J.Middleton
  e-mail: dick@lingbrae.com
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  
  File Name ......................... config.h
  Written By ........................ Dick Middleton
  Date .............................. 06-Nov-12

  Last edit: "Sboy 2012-11-12 2110";  auto updated by emacs 

  Description : 
      Needed to pass values from sboy.c to cmdline.c

*/

#define PACKAGE "sboy"
#define VERSION myVersion()

#define SMAMAC "00:80:25:21:54:E4"
#define DEVMAC "00:01:95:10:A9:93"

extern char *myVersion();

/*
# Local Variables:
# mode:C
# time-stamp-pattern: "30/Sboy[ \t]+%:y-%02m-%02d %02H%02M"
# End:
#                           ===//=== */
