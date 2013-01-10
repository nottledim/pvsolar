<?php

// PHP program
// Copyright (C) 2012 R.J.Middleton
// e-mail: dick@lingbrae.com

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

// File Name ......................... pv_config.php.inc
// Written By ........................ Dick Middleton
// Date .............................. 29-APR-12

// Last-modified: 2013-01-10  08:34:54 on penguin.lingbrae""

// Description : config data for fernietickle

$included_flag = 'INCLUDE_' . basename(__FILE__);
if (defined($included_flag)) {
  return(TRUE);
}
define($included_flag, TRUE);

define('SYSTEM', 'Name of PV System');
define('INCPATH', '/path to include files');
define('DBHOST', 'localhost');
define('DBUSER', 'username');
define('DBPASS', 'password');
define('DBBASE', 'pvsolar');
//
define('KWP', 4);	  // size of pv system, for graph scaling
define('YRMAX', 3000);    // expected annual yield and year chart max
define('UPDELAY', 12000); // delay before fetching data

// Local Variables:
// mode: php
// time-stamp-pattern: "30/Last-modified:[ \t]+%:y-%02m-%02d  %02H:%02M:%02S on %h"
// End:
