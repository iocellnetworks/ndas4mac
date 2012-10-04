#ifndef _LPX_INPUT_H_
#define _LPX_INPUT_H_

/***************************************************************************
 *
 *  lpx_input.h
 *
 *    API definitions for lpx_input.c
 *
 *    Copyright (c) 2003 XiMeta, Inc. All rights reserved.
 *
 **************************************************************************/

/***************************************************************************
 *
 *  API prototypes
 *
 **************************************************************************/

/*
 * LPX input routine.  Pass to next level.
 */
void Lpx_IN_intr (struct mbuf *m);

void Lpx_IN_ctl( int cmd,
                 struct sockaddr *arg_as_sa, /* XXX should be swapped with dummy */
                 void *dummy);

#endif	/* !_LPX_INPUT_H_ */
