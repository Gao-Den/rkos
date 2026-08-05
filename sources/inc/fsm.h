/**
 ******************************************************************************
 * @author: GaoDen
 * @date:   03/05/2024
 ******************************************************************************
**/

#ifndef __LT_FSM_H__
#define __LT_FSM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "message.h"

#define FSM(me, init_func)       ((fsm_t*)me)->state = (state_handler)init_func
#define FSM_TRAN(me, target)     ((fsm_t*)me)->state = (state_handler)target

typedef void (*state_handler)(rk_msg_t*);

typedef struct {
	state_handler state;
} fsm_t;

extern void fsm_dispatch(fsm_t* me, rk_msg_t* msg);

#ifdef __cplusplus
}
#endif

#endif /* __LT_FSM_H__ */
