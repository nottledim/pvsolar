-- phpMyAdmin SQL Dump
-- version 3.4.4deb1
-- http://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: May 02, 2012 at 05:23 PM
-- Server version: 5.1.49
-- PHP Version: 5.3.3-7+squeeze1

SET SQL_MODE="NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;

--
-- Database: `pvsolar`
--

-- --------------------------------------------------------

--
-- Table structure for table `DayData`
--

CREATE TABLE IF NOT EXISTS `DayData` (
  `Date` date NOT NULL,
  `Time` time NOT NULL,
  `CurrentPower` int(11) NOT NULL,
  `ETotalToday` float NOT NULL,
  `Meter` float NOT NULL,
  `DataPoint` smallint(6) NOT NULL,
  `Stamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY `Reading` (`Date`,`DataPoint`),
  KEY `Date` (`Date`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `DayMeta`
--

CREATE TABLE IF NOT EXISTS `DayMeta` (
  `Date` date NOT NULL,
  `Energy` float DEFAULT NULL,
  `Avg12` float DEFAULT NULL,
  `Avg28` float DEFAULT NULL,
  `Meter` float DEFAULT NULL,
  `Info` varchar(512) DEFAULT NULL,
  `Stamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY `Date` (`Date`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `Meters`
--

CREATE TABLE IF NOT EXISTS `Meters` (
  `Date` date NOT NULL,
  `FIT` float NOT NULL,
  `Inverter` float NOT NULL,
  `Avg5` float DEFAULT NULL,
  `Stamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  UNIQUE KEY `Date` (`Date`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
