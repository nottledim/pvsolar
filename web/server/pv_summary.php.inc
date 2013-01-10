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

// File Name ......................... summary.php.inc
// Written By ........................ Dick Middleton
// Date .............................. 21-Jan-12

// Last-modified: 2013-01-10  08:35:41 on penguin.lingbrae""

// Description :

$included_flag = 'INCLUDE_' . basename(__FILE__);
if (defined($included_flag)) {
  return(TRUE);
}
define($included_flag, TRUE);

$monthtxt = array('', 'January', 'Februaray', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December');

function db_error($rtn, $message) {
  if (!$rtn) {
    die($message . ": " . mysql_error() . "\n");
  }
}

function summary() {
  global $monthtxt;

  $link = mysql_connect(DBHOST, DBUSER, DBPASS);
  db_error($link, "Could not connect");
 
  $db_selected = mysql_select_db(DBBASE, $link);
  db_error($db_selected, 'Cannot use database '. DBBASE);

  //----- get latest offset

  $query = "SELECT Avg5 as offset from Meters where Avg5 IS NOT NULL order by Date desc limit 1";
  $result = mysql_query($query);
  if (!$result)  die('Invalid query : ' . mysql_error());
  if (mysql_num_rows($result) > 0) {
    $row = mysql_fetch_assoc($result);
    $offset = $row['offset'];
  }
  else $offset = 1.011;     // just in case

  //----- Annual totals

  $query = "SELECT year(Date) as year, sum(Energy) as ETotalYear FROM DayMeta group by year(Date) order by year(Date) asc limit 10";
  $result = mysql_query($query);
  if (!$result)  die('Invalid query : ' . mysql_error());

  $years = $yr_nrg = array();
  while ($row = mysql_fetch_assoc($result)){
    $yr_nrg[] = round($row['ETotalYear']/$offset, 2);
    $years[] = $row['year'];			
  }

  //----- By month totals

  $query = "SELECT year(Date) as Year, Month(Date) as Month, sum(Energy) as ETotalMonth from DayMeta group by year(Date), Month(Date) order by year(Date) desc , Month(Date) desc LIMIT 12";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());

  $months = $mo_nrg = array();
  $datamax = 0;
  while ($row = mysql_fetch_assoc($result)){
    array_unshift($mo_nrg,  round($row['ETotalMonth']/$offset, 2));
    array_unshift($months, sprintf("%1.3s %d", $monthtxt[$row['Month']], $row['Year']));
    if ($row['ETotalMonth'] > $datamax)  $datamax = $row['ETotalMonth'];  // max value
  }

  //----- By days

  $query = "SELECT Month(Date) as Month, Day(Date) as Day, round(Energy, 2) as Energy FROM DayMeta WHERE Energy IS NOT NULL order by Date desc limit 28";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());

  $datamax = 0;
  $days = $day_nrg = array();
  while ($row = mysql_fetch_assoc($result)){
    array_unshift($day_nrg, +$row['Energy']);
    array_unshift($days, sprintf("%d %1.3s", $row['Day'], $monthtxt[$row['Month']]));
    if ($row['Energy'] > $datamax)  $datamax = $row['Energy'];  // max value
  }

  $query = "SELECT Avg28, Avg12 from DayMeta where Meter IS NOT NULL order by  Date desc limit 1";
  $result = mysql_query($query);
  if (!$result) die('Invalid query : ' . mysql_error());
  $row = mysql_fetch_array($result);
  $avg12 = +$row['Avg12'];
  $avg28 = +$row['Avg28'];

  mysql_close($link);

  return array( 'series' => array ('years' => $years,
				   'yr_nrg' => $yr_nrg,
				   'months' => $months,
				   'mo_nrg' => $mo_nrg,
				   'days' => $days,
				   'day_nrg' => $day_nrg),
		'items' => array ('avg12' => $avg12,
				  'avg28' => $avg28) 
		);
};

// Local Variables:
// mode: php
// time-stamp-pattern: "30/Last-modified:[ \t]+%:y-%02m-%02d  %02H:%02M:%02S on %h"
// End:
