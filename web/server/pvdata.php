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

// File Name ......................... pvdata.php
// Written By ........................ Dick Middleton
// Date .............................. 4-May-12

// Last-modified: 2013-01-10  08:37:12 on penguin.lingbrae""

// Description : get data for pvsolar

if(defined('STDIN') && isset($_SERVER['QUERY_STRING'])) parse_str($_SERVER['QUERY_STRING'], $_GET); // command line debugging

require 'pv_config.php.inc';   // site specific data

date_default_timezone_set("UTC");
$credits =  array( 'enabled' => false );

if (isset($_GET['pv'])) {

  if ($_GET['pv'] == 'day') {
    require INCPATH . '/' . 'pv_daydata.php.inc';
    $dataset = daydata();
  }
  elseif ($_GET['pv'] == 'sum') {
    require INCPATH . '/' . 'pv_summary.php.inc';
    $alldata = summary();
    $dataset = $alldata['items'];
    $series = $alldata['series'];
    $dataset['system'] = SYSTEM;
    $dataset['updelay'] = UPDELAY; 
    $dataset['yrmax'] = YRMAX; 

    $dataset['md_x'] = $series['days'];
    $dataset['md_y'] = $series['day_nrg'];
    $dataset['mo_x'] = $series['months'];
    $dataset['mo_y'] = $series['mo_nrg'];
    $dataset['yr_x'] = $series['years'];
    $dataset['yr_y'] = $series['yr_nrg'];

  }
  else die("must specify correct data type: pvdata.php?pv=<data>\n");
}
else {
  die("must specify type of data: pvdata.php?pv=<data>\n");
}

//header("Content-type: text/json");
header("Content-type: application/json");

echo json_encode($dataset);

// Local Variables:
// mode: php
// time-stamp-pattern: "30/Last-modified:[ \t]+%:y-%02m-%02d  %02H:%02M:%02S on %h"
// End:
