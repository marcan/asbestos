/*  config.h - global AsbestOS configuration options

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com

This code is licensed to you under the terms of the GNU GPL, version 2;
see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#ifndef CONFIG_H
#define CONFIG_H

// Automatically start TFTP download and boot
#define AUTO_TFTP

// Set up a server to control AsbestOS over the network
//#define NETRPC_ENABLE

// Configure the (currently hardcoded) kernel arguments
#define DEFAULT_BOOTARGS "udbg-immortal video=ps3fb:mode:3 root=/dev/nfs rw nfsroot=192.168.3.171:/home/marcansoft/sony/ps3/nfsroot ip=dhcp init=/linuxrc"

#endif
