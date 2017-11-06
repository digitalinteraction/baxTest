// Si44_config.h
// Create locally to define the radio init scripts required
#ifndef _SI44_CFG_H_
#define _SI44_CFG_H_

// For types
#include <stdio.h>
#include "Peripherals/Si44.h"

#define GPIO0_SETTING	0x12
#define GPIO1_SETTING	0x15
#define GPIO2_SETTING	0x00
#define SI44_XTAL_LOAD	0x60

#ifdef BAX_CONFIG_MAKE_VARS
	// Empty setup
	const Si44Cmd_t bax_setup[] = {
		{SI44_CMD_EOL,	0,	NULL}
	};
	// Resume reception command
	const Si44Cmd_t resumeRx[] = {
		{SI44_RX,	1,	"\xff"},
		{SI44_CMD_EOL,			0,	NULL}};
#else
	extern const Si44Cmd_t resumeRx[];
	extern const Si44Cmd_t bax_setup[];
#endif
#endif
