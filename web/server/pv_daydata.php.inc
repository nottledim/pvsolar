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

// File Name ......................... daydata.php
// Written By ........................ Dick Middleton
// Date .............................. 29-APR-12

// Last-modified: 2013-01-10  08:36:05 on penguin.lingbrae""

// Description :

$included_flag = 'INCLUDE_' . basename(__FILE__);
if (defined($included_flag)) {
  return(TRUE);
}
define($included_flag, TRUE);

define('T5MINS', '300000');	// 5 mins in millisecs
define('PERIODS', '12000');     // number of meas periods in hour 
                                // * 1000 to convert to W
//-----

function find_ymax ($peak, $kwp) {
  $mul = 4 / $kwp;   // copes with 1 2 or 4 kWp
  
  $peak *= $mul;

  if ($peak < 425) $peak = 500;
  elseif ($peak < 850) $peak = 1000;
  elseif ($peak < 1350) $peak = 1500;
  elseif ($peak < 1850) $peak = 2000;
  elseif ($peak < 2350) $peak = 2500;
  elseif ($peak < 2850) $peak = 3000;
  else $peak = 4000;

  return $peak /= $mul;
}

function find_y2max($emax) {
  if ($emax < 0.45) $emax = 0.5;
  elseif ($emax < 0.95) $emax = 1;
  elseif ($emax < 1.95) $emax = 2;
  elseif ($emax < 4.95) $emax = 5;
  elseif ($emax < 9.95) $emax = 10;
  elseif ($emax < 14.5) $emax = 15;
  elseif ($emax < 19.5) $emax = 20;
  elseif ($emax < 24.5) $emax = 25;
  elseif ($emax < 29.5) $emax = 30;
  elseif ($emax < 39.5) $emax = 40;
  elseif ($emax < 49.5) $emax = 50;
  else  $emax = 100;		// that would be nice
  return $emax;
}

function pt2tm($index, $date) {
  $hour = floor(($index * 5) / 60);
  $minute = round(((($index * 5) / 60) - $hour) * 60);
  return strtotime(sprintf("$date %02u:%02u",$hour,$minute));
}

function db_error($rtn, $message) {
  if (!$rtn) {
    die($message . ": " . mysql_error() . "\n");
  }
}

function dayDuration($datestr) {
  $dpt = array(0, 250);
  $query = "select DataPoint from DayData where Date='$datestr' and CurrentPower > 0 order by DataPoint asc limit 1";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());
  if (mysql_num_rows($result) > 0) {
    $row = mysql_fetch_row($result);
    $dpt[0] = $row[0];
  }
  $query = "select DataPoint from DayData where Date='$datestr' and CurrentPower > 0 order by DataPoint desc limit 1";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());
  if (mysql_num_rows($result) > 0) {
    $row = mysql_fetch_row($result);
    $dpt[1] = $row[0];
  }
  return ($dpt[1] - $dpt[0]) * 5;  // return minutes
}

function daydata() {

  $link = mysql_connect(DBHOST, DBUSER, DBPASS);
  db_error($link, "Could not connect");
 
  $db_selected = mysql_select_db(DBBASE, $link);
  db_error($db_selected, 'Cannot use database '. DBBASE);

  //-----

  $query= "select max(Date) as latest, min(Date) as earliest from DayData where CurrentPower > 0";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());
  if (mysql_num_rows($result) > 0) {
    $dates = mysql_fetch_array($result);
  
  }

  $match = array();
  if (isset( $_GET['date']) && preg_match("/^(today|yesterday|\d{4}-\d{2}-\d{2})\s*$/",  $_GET['date'], $match)) {
    if ($match[1] == 'today') $datestr = $dates['latest'];
    elseif ($match[1] == 'yesterday') $datestr = date('Y-m-d', strtotime("yesterday"));
    else  $datestr = $match[1];	// valid format anyway
  }
  else $datestr = $dates['latest'];

  if (! $datestr) $datestr = date('Y-m-d'); // just in case

  $title = date('jS F Y', strtotime($datestr));  // chart title

  //-----

  $query = "SELECT MAX(CurrentPower) as peak, round(max(ETotalToday),3) as emax, min(Time) as first, max(Time) as last, round(max(Meter),3) as meter from DayData where Date = '$datestr'";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());
  if (mysql_num_rows($result) > 0) {
    $times = mysql_fetch_array($result);
    $peak = $times['peak'];
    $emax = $times['emax'];
    $meter = $times['meter'];
  }

  //-----

  $query = "SELECT Info FROM DayMeta where Date = '$datestr' and Info IS NOT NULL";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());
  if (mysql_num_rows($result) > 0) {
    $row = mysql_fetch_row($result);
    $info = json_decode($row[0]);
  }
  else  {
    // use defaults
    $info->{'sfrst'} = $info->{'begin'} = strtotime($datestr ." ". $times['first']);   
    $info->{'last'} = $info->{'end'}  = strtotime($datestr ." ". $times['last']);   
    $info->{'ptbegin'} = floor( ($info->{'sfrst'} % (24*60*60)) / (5*60));
    $info->{'ptend'} = floor( ($info->{'last'} % (24*60*60)) / (5*60));
  }
  if (isset($info->{'offset'})) $offset = $info->{'offset'}; else $offset = 1.011;
  $meter = round( $meter / $offset, 3);

  //-----

  $query = "select * from DayData where Date = '$datestr' order by DataPoint asc";

  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());

  $power = array();
  $energy = array();
  $smooth = array();

  $pt = 0;
  $nrg = 0;
  if (mysql_num_rows($result) > 0) {

    while ($row = mysql_fetch_assoc($result)) {
      $dp = $row{'DataPoint'};
      if ($row['ETotalToday'] < $nrg) $row['ETotalToday'] = $nrg; 
      else $nrg = +$row['ETotalToday'];
      if (!$pt) { // first time only
	//      {"first":"1326702600","rise":"1326702840","last":"1326730500",
	//       "evening":1326730800,"end":"1326731400",
	//       "ptend":198,"sfrst":1326702600,"reset":1326769200,
	//       "now":1326672679,"begin":"1326702600","ptbegin":102,"set":"1326730440"}

	// zero initial data points 
	$pt = $info->{'ptbegin'};
	$time = pt2tm($pt, $datestr) * 1000;
	$power[] = array($time, 0);
	$energy[] = array($time, 0);
	$smooth[] = array($time, 0);
	if ($pt < $dp) {
	  $pt = $dp -1;
	  $time = pt2tm($pt, $datestr) * 1000;
	  $power[] = array($time, 0);
	  $energy[] = array($time, 0);
	  $smooth[] = array($time, 0);
	}

	$time += T5MINS;
	$pt++;

	$cp = 0;   // current power
	$en = 0;   // energy
	$sm = 0;   // smoothed (computed)  power
	$smx = 0;  // max smoothed
	$dpt = $pt;
      }

      $diff = $dp - $pt;  // number of missing points
      if ($diff > 0) {  // this is an attempt at interpolation
	$diff++;
	$cpinc = ($row['CurrentPower'] - $cp) / $diff;
	$eninc = ($row['ETotalToday'] - $en) /$diff;
	while ($pt < $dp) {  // put in missing points
	  $cp += $cpinc;
	  $en += $eninc;
	  $dpt = $pt;
	  $power[] = array($time, $cp);
	  $energy[] = array($time, $en);
	  $smooth[] = array($time, $sm);
	  $time += T5MINS;
	  $pt++;
	}
      }

      // normal data point
      $sminc = $en;
      $en = +$row['ETotalToday'];
      $cp = +$row['CurrentPower'];
      $sm = ($en - $sminc) * PERIODS;
      $dpt = $pt;
      if ($sm > $smx) $smx = $sm;
      $power[] =  array($time, $cp);
      $energy[] = array($time, $en);
      $smooth[] = array($time, $sm);
      $time += T5MINS;
      $pt++;
    }

    // put zero data point after last reading
    $power[] =  array($time, 0);
    $energy[] = array($time, +$emax);
    $smooth[] = array($time, 0);

    // ensure the energy line goes to edge of graph
    $pt = $info->{'ptend'};
    $time = pt2tm($pt, $datestr) * 1000;

    $energy[] = array($time, +$emax);

  }

  if ( isset($_GET['smooth']) && $_GET['smooth'] == 1) {
    $use_smooth = 1;
    $peak = find_ymax($smx, KWP);
  }
  else {
    $use_smooth = 0;
    $peak = find_ymax($peak, KWP);
  }
  $elim = find_y2max($emax);

  $now = time();
  if ( ($now < $info->{'first'}) || ( $now > $info->{'last'}) ) {
    $dur = dayDuration($datestr);  // run time minutes
    $e_ave = round($en * 1000 / $dur  * 60, 0);  // W.h
    $e_rtm = sprintf("%dh %02dm", floor($dur/60), $dur % 60);
  }
  else $e_ave = $e_rtm = 0;

  return array('date' => $datestr,
	       'title' => $title,
	       'peak' => $peak, // Y axis 1 range
	       'elim' => $elim, // Y axis 2 range
	       'tmbegin' => $info->{'begin'} * 1000, 
	       'tmend' => $info->{'end'} * 1000, 
	       'first' => $info->{'sfrst'} * 1000,
	       'last' => $info->{'last'} *1000,
	       'latest' => $dates{'latest'},
	       'earliest' => $dates{'earliest'},
	       'srise' => $info->{'rise'} * 1000,
	       'sset' => $info->{'set'} * 1000,
	       'r_pwr' => $cp,  // last readings
	       'r_nrg' => $en,
	       'r_smh' => $sm,
	       'r_dpt' => $dpt,
	       'e_ave' => $e_ave,
	       'e_rtm' => $e_rtm,
	       'meter' => $meter,
	       'energy' => $energy, // arrays
	       'power' => $use_smooth ?  $smooth : $power
	       );
};

// Local Variables:
// mode: php
// time-stamp-pattern: "30/Last-modified:[ \t]+%:y-%02m-%02d  %02H:%02M:%02S on %h"
// End:
