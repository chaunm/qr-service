/*
 * qr-actor.h
 *
 *  Created on: Aug 29, 2016
 *      Author: ChauNM
 */

#ifndef QR_ACTOR_H_
#define QR_ACTOR_H_

#include "Actor/actor.h"

typedef struct tagQRACTOROPTION {
	ACTOROPTION actorOption;
	char* camDevice;
	int imageWidth;
	int imageHeight;
} QRACTOROPTION, *PQRACTOROPTION;

#endif /* QR_ACTOR_H_ */
