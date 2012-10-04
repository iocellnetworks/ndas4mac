#ifndef _LPX_MODULEL_INT_H_
#define _LPX_MODULE_INT_H_

/***************************************************************************
 *
 *  lpx_module_int.h
 *
 *    Internal definitions for lpx_module.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  Helper function prototypes
 *
 **************************************************************************/

void lpx_MODULE_ifnet_attach(const char *name);

int lpx_MODULE_ifnet_detach(const char *name);

#endif	/* !_LPX_MODULE_INT_H_ */
