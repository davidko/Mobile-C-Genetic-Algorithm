#ifndef _CONVO_STATE_MACHINE_H_
#define _CONVO_STATE_MACHINE_H_

#include <mobilec.h>

enum states {
  STATE_IDLE,           /* 0  */
  STATE_REQUESTED_MATE, /* 1  */
  STATE_WAIT_FOR_GENE,  /* 2  */
  STATE_MAX };  

enum events {
  EVENT_REQUEST_MATE,   /* 0  */
  EVENT_AFFIRM,         /* 1  */
  EVENT_REJECT,         /* 2  */
  EVENT_TIMEOUT,        /* 3  */
  EVENT_RECV_GENE,      /* 4  */
  EVENT_ERROR,          /* 5  */
  EVENT_MAX };

typedef struct convo_state_s {
  const char* convo_id;
  int cur_state;
  int time_last_action; /* time() of the last known action on this convo. */
  int timeout; /* Timeout length in seconds */
  const fipa_acl_message_t* acl; /* Last received ACL message */
  struct convo_state_s* next; /* This is a linked list */
} convo_state_t ;

convo_state_t* convo_state_new(char* convo_id);
void convo_state_destroy(convo_state_t* convo);

void init_convo_state_machine();

int insert_convo(convo_state_t** head, convo_state_t* node);
int remove_convo(convo_state_t** head, convo_state_t* node);

/* Return values:
 * 0  - Everything is OK
 * 1  - Terminate the convo normally
 * -1 - Terminate tho convo; there was an error
 *  */
int action_s0_e0(convo_state_t* state);
int action_s0_e3(convo_state_t* state);
int action_s0_e4(convo_state_t* state);
int action_s1_e1(convo_state_t* state);
int action_s1_e2(convo_state_t* state);
int action_s1_e3(convo_state_t* state);
int action_s2_e3(convo_state_t* state);
int action_s2_e4(convo_state_t* state);
int action_handle_error(convo_state_t* state);
int action_invoke_error(convo_state_t* state);

extern int (*const state_table[STATE_MAX][EVENT_MAX]) (convo_state_t* state);

#ifdef _CH_
#pragma importf "convo_state_machine.c"
#endif

#endif
