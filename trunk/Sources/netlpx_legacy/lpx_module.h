#ifndef _LPX_MODULE_H_
#define _LPX_MODULE_H_

/***************************************************************************
 *
 *  lpx_module.h
 *
 *    API definitions for lpx_module.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

#include <net/kext_net.h>


/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

kern_return_t Lpx_MODULE_start (kmod_info_t * ki, void * d);

kern_return_t Lpx_MODULE_stop (kmod_info_t * ki, void *data);

/*
 * LPX initialization.
 */
void Lpx_MODULE_init();

#endif	/* !_LPX_MODULE_H_ */
