/*  config.h - global AsbestOS configuration options

Copyright (C) 2010-2011  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef CONFIG_H
#define CONFIG_H

// Automatically start TFTP download and boot
#define AUTO_TFTP

// Set up a server to control AsbestOS over the network
//#define NETRPC_ENABLE

// Automatically start kernel from HDD
//#define AUTO_HDD

#if defined(AUTO_TFTP) || defined(NETRPC_ENABLE)
#define USE_NETWORK
#endif

#endif
