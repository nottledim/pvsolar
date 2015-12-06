/*
  Sboy - SMA SunnyBoy inverter reader
  Copyright (C) 2012 R.J.Middleton
  e-mail: dick@lingbrae.com
*/

const char *ver = "Sboy 2015-12-06 1331"; /* auto updated by emacs */

/*
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

  File Name ......................... sboy.c
  Written By ........................ Dick Middleton
  Date .............................. 01-Jan-12
  Description :
      Options:  See help info:  sboy -h
  preprocess options:
      gengetopt -i sboy.ggo
  compile:
      gcc -g -DHAVE_CONFIG_H sboy-ng.c cmdline.c -lbluetooth -o sboy

  N.B  Bluetooth default MAC addresses in config.h.
       Use hcitool to lookup your MAC addresses.

  6-Dec-15 RJM added tzOffset option
*/

/* gengetopts workaround */
/* need to set these to 1 or 0 according to option configuration */
/* full help is needed if any hidden options are defined */
/* detailed help is required if and details are given for an option */
#define HAVE_FULL_HELP 1	/* quiet option is hidden */
#define HAVE_DETAILED_HELP 1	/* mac address has details */

//=============
#ifndef ENABLE_ASSERT /* add to cflags in Makefile to enable assert */
#define NDEBUG	   /* defined before includes to inhibit assertions */
#endif
//=============

#include "config.h"		/* bodge to pass data to gengetopts */
#include "cmdline.h"		/* output from gengetopts */
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <assert.h>		/* needed for development only */
#include <sys/select.h>
#include <sys/socket.h>		/* needed for bluetooth comms */
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

#define MACLEN 6		/* size of binary mac address */
#define MACTXTLEN (MACLEN*3-1)	/* size of mac address text */
#define MYNAME PACKAGE		/* defined in config.h */
#define READTO 5		/* Bluetooth read timeout */
#define L1HEADRLEN 18		/* length of L1 header */
#define L2HEADRLEN 28
#define PLEND 3			/* terminating sequence length of L2 payload */
#define FCSINIT 0xffff		/* FCS initial value and mask */
#define NULMAC "FF:FF:FF:FF:FF:FF" /* null mac address */

#define BP_CHAN 4		/* buffer position for channel number */
#define BP_SIGS 4      		/* buffer position for sig strength */
#define BP_RESLT 13		/* buffer posn for results */
#define RES_START (L2HEADRLEN + BP_RESLT) /* from start of L2 data part */

#define L1_INIT 2		/* L1 command for init packet */
#define L1_CMD  1		/* usual L1 command */
#define L1_SIG  3		/* L1 command for BT sig strength */
#define L2STD 0			/* normal bytes for L2 finalise */
#define INIT3 0x0300		/* special bytes for Init 3 L2 finalise */
#define INIT4 0x0100		/* special bytes for Init 4 Logon L2 finalise */
#define EINF MYNAME, __LINE__	/* error info for error message */
#define EP "%s(%i): "		/* error message prefix */

#define PWR_MAX 5000		/* Power limit value for validation */
#define NRG_MAX 50		/* Day Energy limit value for validation */
#define MTR_MAX 150000		/* Meter limit value for validation */

typedef unsigned char byte;	/* a byte */
typedef unsigned short uShort;	/* unsigned short int (2 bytes) */
typedef unsigned int uInt;	/* unsigned int (4 bytes) */
typedef unsigned long uLong;	/* unsigned long (4 bytes on i386) */
typedef enum { false = 0, true = 1 } bool;

typedef bool (*rqDecode)();	/* function type definition */

struct addr_set {		/* structure for mac address */
  char *ident;
  bool ready;			/* flag when usable */
  char text[MACLEN *3];		/* text form */
  byte bin[MACLEN];		/* binary form */
};

struct readings {		/* structure for inverter reading */
  byte   offset;		/* offset into readings - usually zero */
  byte   prefix;		/* don't know always 1 */
  uShort value_type;		/* code for value type */
  byte   len;			/* ? seems to indicate multiple values */
  uInt   datetime;		/* datetime stamp in unknown representation */
  int    value_raw;		/* integer value returned*/
  float  value;			/* float value computed */
};

// --------------------------------------------------

static const byte indicator = 0x7E;	/* begin/end of pdu indicator */

static struct addr_set SmaMacAddr = { "inverter", 0 }, // Inverter BT  address
  DevMacAddr = { "host computer", 0 },	   // Computer BT address
  NullMacAddr = { 0 },			   // Null mac address
  SmaSerial = { 0 },	   // Inverter serial number
  DevSerial = { 0 };		// Fake computer serial number (ID)

static byte l1_header[20];	/* space for L1 pdu header */
static byte l1_payload[1024];   /* space for L1 paload - includes L2 payload */
static byte l2_payload[1024];	/* L2 payload */

static byte *pp1, *pp2, *pl1;	/* pointers into buffers */

static uShort fcs;		/* checksum */
static uShort fcstab[256]  = {	/* checksum coefficients */
  0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
  0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
  0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
  0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
  0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
  0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
  0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
  0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
  0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
  0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
  0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
  0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
  0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
  0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
  0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
  0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
  0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
  0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
  0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
  0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
  0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
  0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
  0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
  0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
  0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
  0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
  0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
  0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
  0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
  0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
  0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
  0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static char *fmt_date[] = { 	/* various date formats */
  "%R%t%d %b %Y", /* time day mon_name year*/
  "%R%t%F",	  /* time date */
  "%F%t%R",	  /* date time */
  "%F",		  /* date only */
  "%d %b %Y",	  /* day mon_name year */
  NULL};

static char *fmt_time[] = { 	/* time formats */
  "%R",		  /* time only */
  "%H%M",
  NULL};

static int skt = -1;		/* socket number */
static byte seqNo = -1;		/* L2 pdu sequence number */
static enum { quiet = 0, normal = 1, verbose,  /* verbosity levels */
	      detailed, debug } verbosity = normal; /* debug not used */
static enum { l_meter = 0, l_events = 1 } listtype = l_meter;
bool btStrength = false;	/* flag set to show signal strength */
bool setTime = false;		/* flag to set inverter time */
int tzOffset = 0;               /* timezone offset seconds from GMT (E is -ve) */
bool history = false;		/* flag set to show history */
bool dcValues = false;		/* flag set to show DC readings */
time_t h_begin, h_finish;
struct readings power = { 0 };	/* current generation rate */
struct readings meter = { 0 };	/* total generated */
struct readings energy = { 0 };	/* yield today */

// --------------------------------------------------

// function returns date part of ver string above (passed to gengetopts)
char *myVersion() {
  char *date = strchr(ver, ' ');
  if (date) date++;
  return date;
}

// --------------------------------------------------

// macros to transfer bytes into C types
#define getE(pb, pE) { getB_r( &(pb), ((byte*)(pE)), sizeof(*(pE)) ); }

#define putE(pb, pE) { putB_s( &(pb), ((byte*)(pE)), sizeof(*(pE)) ); }

#define putE_r(pb, pE) { putB_r( &(pb), ((byte*)(pE)), sizeof(*(pE)) ); }

#define putE_e(pb, pE) { putB_e( &(pb), ((byte*)(pE)), sizeof(*(pE)) ); }

// inline code macros
#define fcheck(fcs, bb)  fcs =  (fcs >> 8) ^ (fcstab[(fcs ^ (bb)) & 0xff])

#define escchr(pb, bb) { if ( (bb) == 0x7d ||	\
                              (bb) == 0x7e ||	\
                              (bb) == 0x11 ||	\
                              (bb) == 0x12 ||	\
                              (bb) == 0x13) {	\
                             *(pb)++ = 0x7d; \
                             *(pb)++ = (bb) ^ 0x20;  } \
                         else *(pb)++ = bb; }

// Verbosity level test macros
#define V_QUIET    (verbosity == quiet)
#define V_NORMAL   (verbosity >= normal)
#define V_VERBOSE  (verbosity >= verbose)
#define V_DETAILED (verbosity >= detailed)
//#define V_DEBUG    (verbosity >= debug)

/* -------------------------------------------------- */

#ifdef NOTUSED
void getB_e(byte **bufr, byte *bbs, int len) {
  int ii, bb;
  for(ii = 0; ii < len; ii++) {
    if (*(*bufr) == 0x7d) {
      (*bufr)++;
      bb = *(*bufr)++ ^ 0x20;
    }
    else
      bb = *(*bufr)++;
    fcheck(fcs, bb);
    (((byte*)(bbs))[ii]) = bb;
  }
}
#endif

// get byte without escape or checksum
void getB_r(byte **bufr, byte *bbs, int len) {
  int ii, bb;
  for(ii = 0; ii < len; ii++) {
    bb = *(*bufr)++;
    (((byte*)(bbs))[ii]) = bb;
  }
}

/* -------------------------------------------------- */

// put multiple bytes
void repeatB(byte **bufr, byte bb, int num) {
  int ii;
  for (ii = 0; ii < num; ii++) {
    fcheck(fcs, bb);
    escchr(*bufr, bb);
  }
}

// put number of bytes according to size of type or variable
void putB_s(byte **bufr, byte *bbs, int len) { /* escape and checksum */
  int ii;
  for (ii = 0; ii < len; ii++) {
    fcheck(fcs, bbs[ii]);
    escchr(*bufr, bbs[ii]);
  }
}

void putB_e(byte **bufr, byte *bbs, int len) { /* escape only */
  int ii;
  for (ii = 0; ii < len; ii++) {
    escchr(*bufr, bbs[ii]);
  }
}

void putB_r(byte **bufr, byte *bbs, int len) { /* completely raw */
  int ii;
  for (ii = 0; ii < len; ii++) {
    *(*bufr) = bbs[ii];
    (*bufr)++;
  }
}

/* ================================================== */

/* replace fprintf(stderr .... */
bool errmsg(char *fmt, ...) {
  va_list fa;
  va_start(fa,fmt);
  vfprintf(stderr, fmt, fa);
  va_end(fa);
  return false;			/* always returns false as a convenience */
}

// prints a sequence of bytes i.e. buffer
// prefix is a text header for identification
void debugBufr(char *prefix, byte *bufr, int len) {

  errmsg("%s--> ", prefix);
  int i;
  for ( i = 0; i < len; i++){
    errmsg("%02X ", bufr[i]);
  }
  errmsg("\n");

}

// check format of text mac address (user input)
int isValidMacAddress(const char* mac) {
  int i = 0;
  int s = 0;

  while (*mac) {
    if (isxdigit(*mac)) {
      i++;
    }
    else if (*mac == ':' || *mac == '-') {

      if (i == 0 || i / 2 - 1 != s)
	break;
      ++s;
    }
    else {
      s = -1;
    }
    ++mac;
  }
  return (i == 12 && (s == 5 || s == 0));
}

/* hex conversion - from smatool.c */
byte conv(char *nn) {

  // ascii index
  //             0  1  2  3  4  5  6  7  8  9
  byte tab[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0, 0,
		 //             A   B   C   D   E   F
		 10, 11, 12, 13, 14, 15 };

  return (tab[(nn[0] - 0x30 ) & ~0x20] << 4) + tab[(nn[1] - 0x30) & ~0x20];
}
/* string to binary */
void convert_addr(const char *str, byte bufr[], int len) {
  char mac[MACTXTLEN];		/* strtok overwrites string */
  strcpy(mac, str);
  len--;
  bufr[len] = conv(strtok(mac, ":"));
  while (len > 0) {
    len--;
    bufr[len] = conv(strtok(NULL, ":"));
  }
}

// ==================================================

/* Bluetooth */
bool BTconnect(int *skt) { /* returns OK */
  struct sockaddr_rc addr = { 0 };
  bool OK = false;

  // set the connection parameters (who to connect to)
  addr.rc_family = AF_BLUETOOTH;
  addr.rc_channel = (uint8_t) 1;

  str2ba(SmaMacAddr.text, &addr.rc_bdaddr );

  // connect to server
  int tries = 2;
  while ( ! OK && (tries > 0)) {

    // allocate a socket
    *skt = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (*skt >= 0) {

      if (0 !=  connect(*skt, (struct sockaddr *)&addr, sizeof(addr))) {
	/* failed to connect to socket - inverter not alive most likely */
	errmsg(EP"Failed to connect to %s: %s\n",
		EINF, SmaMacAddr.text, strerror(errno) );
      }
      else OK = true; 		/* connected OK */
    }
    else 			/* socket allocation failed */
      errmsg(EP"Failed to allocate socket: %s\n",
	      EINF, strerror(errno) );
    if (! OK) {
      close(*skt);
      *skt = -1;
    }
    tries--;
  }

  return OK;
}

bool BTwrite(byte *header, byte hlen, byte *payload, byte plen) {
  bool OK = true;
  ssize_t written;

  if ( (OK = (skt >= 0)) ) {
    if (header != NULL) {
      if (V_VERBOSE) debugBufr("Tx  H", header, hlen);
      written = write(skt, header, hlen);
      OK = (written == hlen);
    }
    if (OK && (payload != NULL)) {
      if (V_VERBOSE) debugBufr("Tx  P", payload, plen);
      written = write(skt, payload, plen);
      OK = (written == plen);
    }
    if (!OK)
      errmsg(EP"Length error with bluetooth write\n", EINF);
  }
  return OK;
}

bool BTread(byte **prx, int *bytes_read) { /* returns OK */
  static byte rxBufr[256];
  static fd_set readfds;
  static struct timeval tval = { 0 };
  bool OK = true ;

  if (skt < 0) {
    OK = BTconnect(&skt);

    if (OK) {

      tval.tv_sec = READTO;   // set timeout of reading
      tval.tv_usec = 0;

      FD_ZERO(&readfds);
      FD_SET(skt, &readfds);
    }
  }

  if (OK) {
    OK = false;
    *bytes_read = 0;
    int rt =  select(skt+1, &readfds, NULL, NULL, &tval);
    if (rt < 0)
      OK = errmsg(EP"Select failed with bluetooth read: %s\n",
		  EINF, strerror(errno) );
    else if (rt == 0)
      OK = errmsg(EP"Timeout on bluetooth read select\n", EINF);
    else {
      if (FD_ISSET(skt, &readfds)) {// did we receive anything within 5 seconds
	*bytes_read = recv(skt, rxBufr, sizeof(rxBufr), 0); /* read data from bluetooth */
	if ( *bytes_read > 0){
	  *prx = rxBufr;
	  //	  if (V_DETAILED) debugBufr("Rx", rxBufr, *bytes_read);
	  OK = true;
	}
	else errmsg(EP"No bytes read from bluetooth socket (bytes= %i)\n", EINF, *bytes_read);
      }
      else
	errmsg(EP"No data read on bluetooth socket\n", EINF);
    }
  }
  return OK;
}
// --------------------
bool parseL1(byte **prx, int rxLen, byte **pplposn, uShort *action) {
  int ii;
  byte csum = 0;
  byte *pr = *prx;
  byte len;
  bool OK = false;

  assert( rxLen > L1HEADRLEN );

  ii = 4;
  while ( ii-- > 0)
    csum ^= *pr++;

  assert( csum == 0 );
  if (csum == 0) {

    pr = *prx +1;		/* go back to len byte */
    getE(pr, &len);
    assert( len > L1HEADRLEN );
    if (len > L1HEADRLEN) {	// a bit daft if it isnt
      pr = *prx + 4;		/* past checksum */
      //      memcpy(&local, pr, MACLEN); /* skip for now */
      pr += MACLEN;
      //      memcpy(&remote, pr, MACLEN); /* skip for now */
      pr += MACLEN;
      getE(pr, action);
      OK = true;
      for (ii = L1HEADRLEN; ii < len; ii++) {
	if (*pr == 0x7d) {
	  pr++;
	  len--;
	  **pplposn = *pr ^ 0x20;
	}
	else
	  **pplposn = *pr;
	(*pplposn)++;
	pr++;
      }
    }
  }
  return OK;
}

bool handleL2Payload(byte *pbuf, int plen, bool *more) {
  bool OK = true;
  uShort l2err, frag;
  byte *pr;
  byte seqRx;

  pr = pbuf + L2HEADRLEN - 5;
  getE(pr, &l2err);
  getE(pr, &frag);
  getE(pr, &seqRx);
  if (seqRx != seqNo)
    OK = errmsg(EP"L2 SeqNo mismatch rx %i Expected: %i\n",
		EINF, seqRx, seqNo);
  if (V_DETAILED) errmsg("L2 Error: %X  L2 fragment: %X\n",
			  l2err, frag);
  if (l2err != 0)
    errmsg(EP"Error returned by L2 (%d)- Command error?\n", l2err);

  *more = (frag > 0);
  if (l2err == 0x14)
    OK = errmsg(EP"Bad request reported by L2\n", EINF);
  else if (l2err == 0x15) {
    printf(EP"No data or end of data \n", EINF);
    *more = false;		/* over rules frag above */
  }
  if (V_DETAILED) errmsg("Frag flag: %i\n", *more);

  fcs = FCSINIT;
  for (pr = pbuf +1; pr < (pbuf + plen - PLEND); pr++)
    fcs = (fcs >> 8) ^ (fcstab[(fcs ^ *pr) & 0xff]);
  fcs = fcs ^ FCSINIT;

  uShort fcsp;
  getE(pr, &fcsp);
  if (fcs != fcsp)
    OK = errmsg(EP"Data error. Rx L2 checksum error."
		" Calc: %04X != Rx: %04X\n", EINF, fcs, fcsp);	
  return OK;
}

bool read_response(bool *rmore) {
  bool OK = false;
  bool more = false;
  byte *prx;
  int len;
  uShort action;

  bool done = false;
  pp1 = l1_payload;
  do {
    if ( (OK = BTread(&prx, &len)) ) {
      action = 0;
      if ( (OK = parseL1(&prx, len, &pp1, &action)) ) {
	if (V_VERBOSE) debugBufr("Rx H1", prx, L1HEADRLEN);
	if (*l1_payload == indicator) { // L2 pdu
	  if (V_VERBOSE) {
	    debugBufr("Rx H2", l1_payload, L2HEADRLEN);
	    debugBufr("Rx P2", l1_payload + L2HEADRLEN,
		      pp1 - (l1_payload + L2HEADRLEN));
	  }
	  if (action == L1_CMD)
	    OK = handleL2Payload(l1_payload, pp1 - l1_payload, &more);
	}
	else			/* L1 packet */
	  if (V_VERBOSE) debugBufr("Rx P1", l1_payload, pp1 - l1_payload);

	switch (action) {	/* what to do next */
	case 1:			/* normal pdu */
	case 2:			/* init pdu */
	case 4:			/* response to bt sig strength */
	case 5:			/* dunno */
	  done = true;		/* no error - return */
	  break;
	case 7: 		
	  /* appears to be error case (not normally seen) -
	     returns bad packet as payload before responding
	     to following packet */
	  //	  done = false;		/* loop to get next packet */
	  pp1 = l1_payload;	/* reset payload pointer ready for next packet */
	  errmsg(EP"Invalid protocol warning:  L1 error returned\n", EINF);
	  break;
	case 8:			/* get next chunk */
	case 0x0C:		/* ignore - read next pdu */
	case 0x0A:		/* ignore - read next pdu */
	  //	  done = false;
	  break;
	default:
	  done = true;		/* quit loop */
	  OK = errmsg(EP"Data error. Unknown L1 command\n", EINF);
	}
	if (V_DETAILED) errmsg("action: %i\n", action);
      }
      else errmsg(EP"Data error. L1 parse failed\n", EINF);
    }
    else if (V_DETAILED) errmsg(EP"Read from Bluetooth failed\n", EINF);
  } while (OK && !done );
  if (rmore) *rmore = more;
  return OK;
}

/* ================================================== */

/* L2 pdu data sequences */
const byte l2_preamble[] = { 0xFF, 0x03, 0x60, 0x65 };
const byte rq_preamble[] = { 0x80, 0, 2, 0 };

void initializePdu(char *text) {
  pp1 = l1_payload;
  pp2 = l2_payload;
  if (V_VERBOSE && text) {
    errmsg(text);
    errmsg("\n");
  }
}
uShort finaliseL1pdu() {	/* just so it looks consistent */
  return pp1 - l1_payload;	/* returns L1 payload length */
}

uShort finaliseL2pdu(uShort inum) {	/* returns L2 pdu length */
  int len = pp2 - l2_payload + 23;
  byte blen = len / 4;		/* number of 4 byte blocks */
  if (len % 4) blen++;
  if (V_DETAILED) errmsg("len: %i blen: %i\n", len, blen);

  fcs = FCSINIT;

  seqNo++; 			/* increment L2 pdu sequence number */
  *pp1++ = indicator;

  putE(pp1, &l2_preamble);
  putE(pp1, &blen);

  if (SmaSerial.ready) {
    repeatB(&pp1, 0x80, 1);
    putE(pp1, &SmaSerial.bin);	/* recovered serial number */
  }
  else {
    repeatB(&pp1, 0xA0, 1);
    putE(pp1, &NullMacAddr.bin); /* default serial number */
  }
  putE(pp1, &inum);		/* code for something */

  if (DevSerial.ready)		/* it's not really optional */
    putE(pp1, &DevSerial.bin)
  else
    putE(pp1, &NullMacAddr.bin); /* default serial number */

  repeatB(&pp1, 0, 6); 		/* 6 zeros */

  putE(pp1, &seqNo);

  putB_s(&pp1, l2_payload, pp2 - l2_payload);

  fcs ^= FCSINIT;
  putE_e(pp1, &fcs);

  *pp1++ = indicator;

  return pp1 - l1_payload;
}

// this takes L1/L2 payload and sends it as L1 pdu
bool sendPdu(uShort l1cmd, uShort payload_len, rqDecode decode) {
  uShort len = L1HEADRLEN + payload_len;
  bool OK;
  pl1 = l1_header;
  *pl1++ = indicator;
  putE_r(pl1, &len);
  *pl1++ = l1_header[0] ^ l1_header[1] ^ l1_header[2];

  if (DevMacAddr.ready)		/* source address */
    putE_r(pl1, &DevMacAddr.bin)
    else
      putE_r(pl1, &NullMacAddr.bin);

  if (SmaMacAddr.ready)		/* dest address */
    putE_r(pl1, &SmaMacAddr.bin)
    else
      putE_r(pl1, &NullMacAddr.bin);

  putE_r(pl1, &l1cmd);
  assert( (pl1 - l1_header) == L1HEADRLEN);

  OK = BTwrite(l1_header, pl1 - l1_header, l1_payload, payload_len);
  if (decode != NULL) {		/* null if no response expected */
    bool more = true;
    while (OK && more) {
      OK = read_response(&more);
      if (OK) 
	OK = decode();
    }
  }
  return OK;
}

/* ================================================== */

bool dxNoData() { return true; }   /* this is a null data decoder  */

bool dxInit2() {
  /* get serial number from inverter response */
  byte *pv = l1_payload + L2HEADRLEN + 27;
  getE(pv, &SmaSerial.bin);
  SmaSerial.ready = true;
  //    debugBufr("SmaSerial", SmaSerial.bin, MACLEN);
  return true;
}

// Inverter initialization and logon process
bool init_session() {
  bool OK =false;
  static byte init_netid_ack[] = { 0x00, 0x04, 0x70, 0x00 };
  static byte init_neg_part2[] = { 0x80, 0x0E, 0x01, 0xFD, 0xFF,
				   0xFF, 0xFF, 0xFF, 0xFF };

  static byte init_negotiate[] = { 0x01, 0x00, 0x00, 0x00 };
  static byte init_logon[] = { 0x80, 0x0c, 0x04, 0xfd,
			       0xff, 0x07, 0x00, 0x00,
			       0x00, 0x84, 0x03, 0x00,
			       0x00, 0xaa, 0xaa, 0xbb, 0xbb };

  static byte init_pwd[12] = { 0xB8, 0xB8, 0xB8, 0xB8, // (char '0' ^ 88) % 0xff
			       0x88, 0x88, 0x88, 0x88, // (zero ^ 88) % 0xff
			       0x88, 0x88, 0x88, 0x88 };


  OK = read_response(NULL);		/* hello from inverter */
  byte chanid = *(l1_payload + BP_CHAN); /* recover chan number */
  if (V_VERBOSE) errmsg(EP"Channel id: %i\n", EINF, chanid);

  if (OK) {
    initializePdu("Init 1");
    putE_r(pp1, &init_netid_ack);
    putE_r(pp1, &chanid);
    repeatB(&pp1, 0, 4);

    putE_r(pp1, &init_negotiate);

    OK = sendPdu(L1_INIT, finaliseL1pdu(), dxNoData);
  }
  //******************************

  if (OK) {
    initializePdu("Init 2");
    SmaMacAddr.ready = true;	/* use mac address in headers */
    putE_r(pp2, &rq_preamble);
    repeatB(&pp2, 0, 9);
    OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), dxInit2);
  }
  //******************************

  if (OK) {
    initializePdu("Init 3 (n.b. no reply)");
    putE_r(pp2, &init_neg_part2);
    OK = sendPdu(L1_CMD, finaliseL2pdu(INIT3), NULL);
    // no response for this one
  }

  //******************************

  if (OK) {
    initializePdu("Init 4 Logon");
    putE_r(pp2, &init_logon);
    repeatB(&pp2, 0, 4);
    putE_r(pp2, &init_pwd);
    OK = sendPdu(L1_CMD, finaliseL2pdu(INIT4), dxNoData);
  }

  return OK;
}

// ==================================================

void print_readings(char *prefix, struct readings *reading) {
  if (V_DETAILED) {
    debugBufr(prefix, l1_payload+L2HEADRLEN+13, pp1 - l1_payload);
    errmsg("\tprefix: %i ", reading->prefix);
    errmsg("type: %04X ", reading->value_type);
    errmsg("len: %02X ", reading->len);
    errmsg("datetime: %i ", reading->datetime);
    errmsg("value: %i\n", reading->value_raw);
  }
}

void decode_readings(struct readings *reading) {
  byte *pv;
  pv = l1_payload + RES_START + reading->offset;
  getE(pv, &reading->prefix);	      /* first byte is 1 */
  getE(pv, &reading->value_type);
  getE(pv, &reading->len);	      /* number of following values (ignored here) */
  getE(pv, &reading->datetime);
  getE(pv, &reading->value_raw);
  reading->value = (float) reading->value_raw / 1000.0;
}

void fmttime(time_t time, char *tbuf, int tbuflen) {
  struct tm *tmp;

  if (time != 0) {
    tmp = gmtime(&time);
    strftime(tbuf, tbuflen, "%H:%M %Z %e %b %Y", tmp);
  }
  else tbuf[0] = 0;
}

time_t str2time(char *timestr, time_t *copy, char **fmt) {
  struct tm d1;
  char *nxtch = NULL;
  char **fp = fmt;

  memset(&d1, 0, sizeof(struct tm));

  time_t now = copy ? *copy : time(NULL);
  gmtime_r(&now, &d1);
  if (copy) {			/* set up defaults */
    d1.tm_hour = 21;		/* set to end of daylight */
    d1.tm_min = 30;
  }
  else {
    d1.tm_hour = 2;		/* set to early */
    d1.tm_min = 30;
  }
  d1.tm_sec = 0;

  time_t td = 0;

  int today = timestr && (strcmp(timestr, "today") == 0);
  int yesterday = timestr && (strcmp(timestr, "yesterday") == 0);

  if (timestr && !(today || yesterday)) {
  /* check through list of formats */
    while ( ((nxtch == NULL) || *nxtch) && (*fp) ) {
      nxtch = strptime(timestr, *fp++, &d1);
    }
  }
  else {
    d1.tm_isdst = 0;		/* we work in GMT only */
    if (yesterday) d1.tm_mday--;
    td = mktime(&d1);
    if (td < 0) td = 0;
  }
  if ((nxtch != NULL) && (*nxtch == 0)) {		/* matching string found */
    d1.tm_isdst = 0;		/* we work in GMT only */
    if (yesterday) d1.tm_mday--;
    td = mktime(&d1);
    if (td < 0) td = 0;
  }
  return td;			/* zero if no match */
}
// -------------------- Set Time
bool setInverterTime() {
  static byte set_time_args[]= { 0x8c, 0x0a, 0x02, 0x00, 0xf0, 0x00,
				 0x6d, 0x23, 0x00, 0x00, 0x6d, 0x23,
				 0x00, 0x00, 0x6d, 0x23, 0x00 };
  static byte set_time_end[] = { 0x01, 0, 0, 0 };
  char tbuf[50];
  bool OK = false;

  uInt now = time(NULL);	/* must be 4 bytes */
  assert(sizeof(now) == 4);

  initializePdu("Set inverter time ");

  putE_r(pp2, &set_time_args);
  putE_r(pp2, &now);
  putE_r(pp2, &now);
  putE_r(pp2, &now);
  putE_r(pp2, &tzOffset);	/* 4 bytes */
  putE_r(pp2, &now);
  putE_r(pp2, &set_time_end);

  fmttime(now, tbuf, sizeof(tbuf));
  if (!V_QUIET) printf("Time set to: %s\n", tbuf);
  if (!setTime) errmsg(EP"Time auto reset to: %s\n", EINF, tbuf);

  OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), NULL); /* no reply expected */
  return OK;
}

bool verifyTime(int dt) {
  bool OK = true;
  if (dt) {	/* check inverter time */
    int diff = dt - (int) time(NULL);
    if (abs(diff) > 30) {
      errmsg(EP"Inverter time incorrect (%is)\n", EINF, diff);
      OK = setInverterTime();	/* set time if wrong */
    }
  }
  else 
    OK = errmsg(EP"No time value for verify\n", EINF);

  return OK;
}
// -------------------- Signal Strength
bool dxSignalStrength() {
  float strength = (100.0 / 256.0) * l1_payload[BP_SIGS];
  if (V_NORMAL) printf("Bluetooth Sig Strength: %.1f%%\n", strength);
  return true;
}

bool rqSignalStrength() {
  static byte params[2] = { 0x05, 0 };
  initializePdu("Rq signal strength");
  putE_r(pp1, &params);
  return sendPdu(L1_SIG, finaliseL1pdu(), dxSignalStrength);
}
// -------------------- DC Readings
bool dxDCreadings() {
  struct readings volts = { 0 };
  struct readings current = { 28, 0 };

  decode_readings(&volts);    // 0x451f = DC Voltage  /100
  decode_readings(&current);  // 0x4521 = DC Current  /1000

  printf("DC volts: %.1fV  current: %.3fA   power: %.3fW\n",
	 volts.value * 10, current.value, volts.value * current.value * 10);
  return true;
}

bool rqDCreadings() {
  static byte inverter_dc_voltamp[] = { 0x83, 0x00, 0x02, 0x80, 
					0x53, 0x00,
					0x00, 0x45, 
					0x00, 0xFF, 
					0xFF, 0x45,
					0x00 };
  bool OK =false;

  initializePdu("Rq inverter DC power");
  putE_r(pp2, &inverter_dc_voltamp);

  OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), dxDCreadings);
  return OK;
}

// -------------------- History
bool dxHistory() {
  time_t dt;			/* time as time_t */
  int dti = 0, value = 0;	/* returned time & value */
  int eval = 0;
  float meter;			/* value converted to float */
  byte *pp = l1_payload + RES_START; /* pointer into payload */
  while ( pp < pp1 - PLEND) {
    getE(pp, &dti);
    getE(pp, &value);
    pp += sizeof(int);		/* skip unknown value */

    dt = (time_t) dti;	/* time_t is 8 bytes on some systems */
    meter = (float) value / 1000.0;
    char tbuf[50];	/* buffer for formatted time/date */
    fmttime(dt, tbuf, sizeof(tbuf));
    printf("%s  %.3f\n", tbuf, meter);
  }
  return true;
}

bool rqHistory() {
  uInt next, until;		/* need to be 32 bits */
  bool OK;		/* result code */

  char tbuf[50];		/* buffer for time/date format */
  fmttime(h_begin, tbuf, sizeof(tbuf));
  printf("History - list from: %s ", tbuf);
  fmttime(h_finish, tbuf, sizeof(tbuf));
  printf("to: %s \n", tbuf);

  next = h_begin;
  OK = (next < h_finish);
  while (OK && (next < h_finish))  {
    until = next + (24 * 3600);
    if (until > h_finish) until = h_finish;

    initializePdu("Rq history");
    putE_r(pp2, &rq_preamble);

    repeatB(&pp2, 0x70, 1);

    assert(sizeof(next) == 4);	/* time must be 4 bytes */
    putE_r(pp2, &next);
    putE_r(pp2, &until);

    OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), dxHistory);
    next = until;		/* start another request (maybe) */
  }
  return OK;
}
#ifdef EVENTS
//--- Events history
bool dxEvents() {
  time_t dt;			/* time as time_t */

  int dti = 0;			/* returned time */
  uShort eid;			/* EntryID   2 */
  uShort sid;			/* SUSyID    2 */
  uInt  sno;			/* SerNo     4 */
  uShort eco;			/* EventCode 2 */
  uShort flg;			/* Flags     2 */
  /* 16 bytes to here */
  uInt Group;			/* 4 */
  uInt uInt1;			/* 4 */
  uInt uInt2;			/* 4 */
  uInt Counter;			/* 4 */
  uInt uInt3;			/* 4 */
  uInt uInt4;			/* 4 */
  uInt uInt5;			/* 4 */
  uInt uInt6;			/* 4 */
  /*this lot 32 bytes */

  byte *pp = l1_payload + RES_START; /* pointer into payload */
  while ( pp < pp1 - PLEND) {
    //    debugBufr("ev",  pp, 48);
    getE(pp, &dti);
    getE(pp, &eid);
    getE(pp, &sid);
    getE(pp, &sno);
    /*    getE(pp, &eco);
    getE(pp, &flg);

    getE(pp, &Group);
    getE(pp, &uInt1);
    getE(pp, &uInt2);
    getE(pp, &Counter);
    getE(pp, &uInt3);
    getE(pp, &uInt4);
    getE(pp, &uInt5);
    getE(pp, &uInt6);*/
    

    dt = (time_t) dti;	/* time_t is 8 bytes on some systems */

    char tbuf[50];	/* buffer for formatted time/date */
    fmttime(dt, tbuf, sizeof(tbuf));
    printf("%s  %04x  %04x  %08x\n", tbuf, eid, sid, sno);
    /*  %04x  %04x | ", tbuf, eid, sid, sno, eco, flg);
    printf("%08x %08x %08x %08x | ", Group, uInt1, uInt2, Counter);
    printf(" %08x %08x %08x %08x\n", uInt4,  uInt4,  uInt5,  uInt6);*/
  }
  return true;
}

bool rqEvents() {
  uInt next, until;		// need to be 32 bits 
  bool OK;			// result code 
  static byte events_args[] = {  0x70, 0x20, 0x02, 0x00 };//, 0x70 };

  char tbuf[50];		// buffer for time/date format
  fmttime(h_begin, tbuf, sizeof(tbuf));
  printf("Events - list from: %s ", tbuf);
  fmttime(h_finish, tbuf, sizeof(tbuf));
  printf("to: %s \n", tbuf);

  next = h_begin;
  OK = (next < h_finish);
  while (OK && (next < h_finish))  {
    until = next + (24 * 3600);
    if (until > h_finish) until = h_finish;

    initializePdu("Rq events");
    putE_r(pp2, &rq_preamble);

    //putE_r(pp2, &eva);

    assert(sizeof(next) == 4);	// time must be 4 bytes
    putE_r(pp2, &next);
    putE_r(pp2, &until);

    OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), dxEvents);
    next = until;		// start another request (maybe)
  }
  return OK;
}
#endif
// -------------------- Current Power
bool dxCurrentPower() {
  decode_readings(&power);
  assert(power.value_type == 0x263F);          /* should be 263F */
  print_readings("CPowr", &power);
  if (V_NORMAL) printf("Current power = %i Watt\n", power.value_raw);
  return (power.value_raw >= 0 && power.value_raw < PWR_MAX) ||
    errmsg(EP"Power value unlikely %i\n", EINF, power.value_raw);
}

bool rqCurrentPower() {
  static byte current_power_args[] = { 0x51,
				       0x00, 0x3f, 0x26, 0x00,
				       0xFF, 0x3f, 0x26, 0x00, 0x0e };
  initializePdu("Rq current power");
  putE_r(pp2, &rq_preamble);
  putE_r(pp2, &current_power_args);

  bool OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), dxCurrentPower);

  return OK;
}
// -------------------- Energy Today
bool dxEnergyToday() {
  decode_readings(&energy);
  assert(energy.value_type == 0x2622); /* should be 2622 */
  print_readings("Enrgy", &energy);
  if (V_NORMAL) printf("E total today = %.3f kW.h\n", energy.value);
  return (energy.value >= 0 && energy.value < NRG_MAX) || 
      errmsg(EP"Energy value unlikely: %.3f\n", EINF, energy.value);
}

bool rqEnergyToday(struct readings *energy) {
  static byte energy_today_args[] = { 0x54,
				      0x00, 0x22, 0x26, 0x00,
				      0xFF, 0x22, 0x26, 0x00 };
  initializePdu("Rq energy today");
  putE_r(pp2, &rq_preamble);
  putE_r(pp2, &energy_today_args);

  bool OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), dxEnergyToday);
  return OK;
}
// -------------------- Meter
bool dxInverterMeter() {
  char tbuf[50];
  decode_readings(&meter);
  assert(meter.value_type == 0x2601); /* should be 2601 */
  print_readings("Meter", &meter);
  if (V_NORMAL) {
    printf("Meter total = %.2f kW.h\n", meter.value);
    fmttime(meter.datetime, tbuf, sizeof(tbuf));
    printf("Time of reading: %s\n", tbuf);
  }
  return (meter.value >= 0 && meter.value < MTR_MAX) || 
      errmsg(EP"Meter value unlikely: %.1f\n", EINF, meter.value);
}

bool rqInverterMeter(struct readings *meter) {
  static byte inverter_meter_args[] = { 0x54, 
					0x00, 0x01, 0x26, 0x00,
					0xff, 0x01, 0x26, 0x00 };
  initializePdu("Rq inverter meter");
  putE_r(pp2, &rq_preamble);
  putE_r(pp2, &inverter_meter_args);

  bool OK = sendPdu(L1_CMD, finaliseL2pdu(L2STD), dxInverterMeter);
  return OK;
}

// ==================================================

bool takeReadings() {

  bool OK = false;

  OK = init_session();		/* comms initialization/negotiation */

  if (setTime)
    OK = OK && setInverterTime();

  if (V_NORMAL) {
    if (btStrength)
      OK = OK && rqSignalStrength();

    if ( dcValues)
      OK = OK && rqDCreadings();
  }

  if (V_NORMAL && history) {
    switch (listtype) {
    case l_meter:
      OK = OK && rqHistory();
      break;
    case l_events:
      //      OK = OK && rqEvents();
      break;
    }
  }
  else {			/* not if doing history */

    OK = OK && rqCurrentPower();

    OK = OK && rqEnergyToday(&energy);
  }

  OK = OK && rqInverterMeter(&meter);

  if (V_QUIET && OK) printf("READINGS: %d: %.3f: %.3f: %s\n",
			    power.value_raw,
			    energy.value,
			    meter.value,
			    ver);

  OK = OK && verifyTime(meter.datetime); /* uses time returned from meter above */
  return OK;
}

bool help_print_workaround(struct gengetopt_args_info *args_info) {
    bool help = false;
    if (args_info->help_given) {
      help = true;
      cmdline_parser_print_help();
    }
#if HAVE_DETAILED_HELP /* need to enable this if an option has details */
    if (args_info->detailed_help_given) {
      help = true;
      cmdline_parser_print_detailed_help();
    }
#endif
#if HAVE_FULL_HELP /* need to enable this if any hidden options defined */
    if (args_info->full_help_given) {
      help = true;
      cmdline_parser_print_full_help();
    }
#endif
    return help;
}

bool verifyMacAddress(struct addr_set *addr, char *addr_default,
		      int addr_given, char *addr_arg, const char *addr_help) {
  static char *optfmt = "%s: Invalid MAC address format for %s\n\n";
  static char *dflfmt = "%s: Invalid default MAC address (%s) for %s. See options definition\n\n";
  static char *bthelp = "Bluetooth mac addresses must be 6 colon separated hex pairs\n";
  static char *optinf = "Option usage: %s\n";

  bool OK = false;
  char *address = (addr_given) ? addr_arg : addr_default;

  if (isValidMacAddress(address)) {
    strncpy(addr->text, address, MACLEN*3);
    addr->text[MACLEN*3-1] = 0;
    OK = true;
  }
  else {
    if (addr_given)
      errmsg(optfmt, MYNAME, addr->ident);
    else
      errmsg(dflfmt, MYNAME, address, addr->ident);
    OK = false;
    errmsg(optinf, addr_help);
    errmsg(bthelp);
  }
  return OK;
}

bool process_options(int argc, char** argv, bool *help) {
  bool OK = true;
  struct gengetopt_args_info args_info;
  /* let's call our cmdline parser */
  if (cmdline_parser (argc, argv, &args_info) == 0) {
    //------------------------------
    if ( (*help = help_print_workaround(&args_info)) ) // Workaround
      puts("\n");		 // need to do this because of a bug
    else {			 // in gengetopts group formatting
    //------------------------------

      /* Option processing */

      verbosity = args_info.verbosity_given ? args_info.verbosity_arg : quiet;

      if (V_DETAILED) errmsg("Verbosity: %s=%i\n", /* display verbosity setting */
			      cmdline_parser_verbosity_values[verbosity], verbosity);

      OK = OK &&  verifyMacAddress(&SmaMacAddr, SMAMAC, args_info.address_given,
				   args_info.address_arg,  args_info.address_help);
      OK = OK &&  verifyMacAddress(&DevMacAddr, DEVMAC, args_info.device_given,
				   args_info.device_arg,  args_info.device_help);

      if (OK && args_info.history_given) {
	OK = false;
	char *msg = "date";
	history = true;

	if ( (h_begin = str2time(args_info.history_arg, NULL, fmt_date)) ) {
	  h_finish = str2time(NULL, &h_begin, fmt_date);

	  if (args_info.start_time_given) {
	    h_begin = str2time(args_info.start_time_arg, &h_begin, fmt_time);
	  }

	  msg = "start time";
	  if ( h_begin &&  args_info.end_time_given) {
	    msg = "end time";
	    h_finish = str2time(args_info.end_time_arg,
			       &h_begin, fmt_time);
	  }
	
	  if (h_finish && h_begin){
	    msg = "end time: begin after end";
	    if (h_finish > h_begin) {
	      OK = true;

	      //	      listtype = args_info.list_type_given ? args_info.list_type_arg : l_meter;
	    }
	  }
	}
	if (!OK) {
	  errmsg(EP"Incorrect format given for history %s\n", EINF, msg);
	}
      }
      /* Time options */
      setTime = args_info.set_time_given;
      if (setTime)
	tzOffset = args_info.set_time_arg;
      
      /* simple options */
      btStrength = args_info.bt_power_given;
      dcValues = args_info.dc_readings_given;

      printf("tz: %d\n", tzOffset);
      /* unless -q, if o/p option given then set -v to normal */
      if (!args_info.quiet_given && V_QUIET && (btStrength || dcValues || history))
	verbosity = normal;	
    }
    cmdline_parser_free (&args_info); /* release allocated memory */
  }
  else
    OK = errmsg(EP"Command line parser error\n", EINF);
  return OK;
}

int main(int argc, char** argv) {
  bool OK;
#ifndef NDEBUG			       /* will tell you if assert is */
  errmsg("Warning: ASSERT ENABLED\n"); /* enabled unwittingly */
#endif
  /* initialize default addresses */
  strcpy(SmaSerial.text, NULMAC); /* this gets replaced from inverter */
  strcpy(NullMacAddr.text, NULMAC);
  convert_addr(NullMacAddr.text, NullMacAddr.bin, MACLEN);
  NullMacAddr.ready = true;

  /* process command line options */
  bool help; 			/* set if just help given */
  OK = process_options(argc, argv, &help);
  if (OK & !help) {
    /* Update address structures */

    /* copy of mac address as fake serial */
    strcpy(DevSerial.text, DevMacAddr.text); 
    
    /* SmaMacAdd not used in initial transactions so ready not set*/
    convert_addr(SmaMacAddr.text, SmaMacAddr.bin, MACLEN); 

    convert_addr(DevMacAddr.text, DevMacAddr.bin, MACLEN); 
    DevMacAddr.ready = true;

    /* SmaSerial obtained from inverter */
    convert_addr(SmaSerial.text, SmaSerial.bin, MACLEN);

    convert_addr(DevSerial.text, DevSerial.bin, MACLEN); 
    DevSerial.ready = true;
	
    if (V_NORMAL)
      puts(ver);	/* output program version string */

    if (V_VERBOSE)
      printf("Address L: %s -> R: %s\n",
	     DevMacAddr.text, SmaMacAddr.text);


    /* the work begins here */
    OK = takeReadings();
  }

  if (!OK) errmsg(EP"Terminated with error\n", EINF);
  return OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
# Local Variables:
# mode:C
# time-stamp-pattern: "30/Sboy[ \t]+%:y-%02m-%02d %02H%02M"
# End:
#                           ===//=== */
