#ifndef ULTRA64_MOTOR_H
#define ULTRA64_MOTOR_H

#include "pfs.h"

#define MOTOR_START 1
#define MOTOR_STOP 0

#define osMotorStart(x) __osMotorAccess((x), MOTOR_START)
#define osMotorStop(x) __osMotorAccess((x), MOTOR_STOP)

#ifdef __cplusplus
extern "C" {
#endif

int32_t __osMotorAccess(OSPfs* pfs, uint32_t vibrate);
int32_t osMotorInit(OSMesgQueue* ctrlrqueue, OSPfs* pfs, int32_t channel);

#ifdef __cplusplus
}
#endif
#endif
