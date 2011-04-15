#ifndef _CONVO_STATE_MACHINE_H_
#define _CONVO_STATE_MACHINE_H_

#include <mobilec.h>

//#define DEBUG

#ifdef DEBUG
#define DEBUGMSG \
  printf("%s:%s:%s:%d\n", mc_agent_name, __func__, __FILE__, __LINE__)
#else
#define DEBUGMSG
#endif


#define PROBABILITY_MATE_SUPERIOR 0.75
#define PROBABILITY_MATE_INFERIOR 0.25
#define AGENT_RESPONSE_WAIT_TIME 5
enum states {
  STATE_IDLE,           /* 0  */
  STATE_REQUESTED_MATE, /* 1  */
  STATE_WAIT_FOR_GENE,  /* 2  */
  STATE_WAIT_FOR_AGENT_LIST, /* 3  */
  STATE_WAIT_FOR_INIT_MATE,  /* 4  */
  STATE_MAX };  

enum events {
  EVENT_REQUEST_MATE,   /* 0 , must also include fitness */
  EVENT_AFFIRM,         /* 1  */
  EVENT_REJECT,         /* 2  */
  EVENT_TIMEOUT,        /* 3  */
  EVENT_RECV_GENE,      /* 4 , must include gene */
  EVENT_ERROR,          /* 5  */
  EVENT_AGENT_LIST,     /* 6 , Receiving an agent list */
  EVENT_REQUEST_FITNESS,/* 7  */
  EVENT_RECEIVE_FITNESS,/* 8  */
  EVENT_MAX };

typedef struct convo_state_s {
  const char* convo_id;
  int cur_state;
  int time_last_action; /* time() of the last known action on this convo. */
  int timeout; /* Timeout length in seconds */
  fipa_acl_message_t* acl; /* Last received ACL message */
  struct convo_state_s* next; /* This is a linked list */
} convo_state_t ;

convo_state_t* convo_state_new(const char* convo_id);
void convo_state_destroy(convo_state_t* convo);

typedef struct agent_info_s {
  char* name;
  double fitness;
} agent_info_t;

agent_info_t* agent_info_new(const char* name, double fitness);
void agent_info_destroy(agent_info_t* agent_info);
int compare_agent_info(const void* _a, const void* _b);

void init_convo_state_machine();

int insert_convo(convo_state_t** head, convo_state_t* node);
int remove_convo(convo_state_t** head, convo_state_t* node);
void init_mate_proposal(const char* name);

/* Return values:
 * 0  - Everything is OK
 * 1  - Terminate the convo normally
 * -1 - Terminate tho convo; there was an error
 *  */
int action_s0_e0(convo_state_t* state);
int action_s0_e3(convo_state_t* state);
int action_s0_e4(convo_state_t* state);
int action_s0_e7(convo_state_t* state);
int action_s0_e8(convo_state_t* state);
int action_s1_e1(convo_state_t* state);
int action_s1_e2(convo_state_t* state);
int action_s1_e3(convo_state_t* state);
int action_s2_e3(convo_state_t* state);
int action_s2_e4(convo_state_t* state);
int action_s3_e6(convo_state_t* state);
int action_s4_e3(convo_state_t* state);
int action_handle_error(convo_state_t* state);
int action_invoke_error(convo_state_t* state);

extern int (*const state_table[STATE_MAX][EVENT_MAX]) (convo_state_t* state);
extern double g_fitness;
extern agent_info_t *g_agent_info_entries[20];
extern int g_num_agent_info_entries;
extern convo_state_t* g_convo_state_head;
extern int g_num_rejects;

#ifdef _CH_
#pragma importf "convo_state_machine.c"
#endif

#endif
