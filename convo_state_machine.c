#include "convo_state_machine.h"

extern double gene[20]; /* Global agent gene */

int (*const state_table[STATE_MAX][EVENT_MAX]) (convo_state_t* state);

void init_convo_state_machine()
{
  int i, j;
  memset(state_table, 0, sizeof(state_table));
  /* Load 'invoke error' as default action for all events */
  for(int i = 0; i < STATE_MAX; i++) {
    for(int j = 0; j < EVENT_MAX; j++) {
      state_table[i][j] = action_invoke_error;
    }
  }

  state_table[0][0] = action_s0_e0;
  state_table[0][3] = action_s0_e3;
  state_table[1][1] = action_s1_e1;
  state_table[1][2] = action_s1_e2;
  state_table[1][3] = action_s1_e3;
  state_table[2][3] = action_s2_e3;
  state_table[2][4] = action_s2_e4;
  for(i = 0; i < STATE_MAX; i++) {
    state_table[i][EVENT_ERROR] = action_handle_error;
  }
}

convo_state_t* convo_state_new(int convo_id)
{
  convo_state_t* convo;
  convo = (convo_state_t*)malloc(sizeof(convo_state_t));
  convo->convo_id = convo_id;
  convo->cur_state = 0;
  convo->time_last_action = time(NULL);
  convo->timeout = 0;
  convo->acl = NULL;
  return convo;
}

void convo_state_destroy(convo_state_t* convo)
{
  free(convo);
}

int insert_convo(convo_state_t* head, convo_state_t* node)
{
  /* Just stick it onto the head */
  convo_state_t* tmp;
  tmp = head;
  head = node;
  node->next = tmp;
  return 0;
}

int remove_convo(convo_state_t** head, convo_state_t* node)
{
  /* First see if the head matches */
  if(node == head) {
    *head = *head->next;
    return 0;
  }
  /* If it's not the head, traverse the list */
  while(head->next != NULL) {
    if(head->next == node) {
      head->next = head->next->next;
      return 0;
    }
    head = head->next;
  }
  return 1;
}


/* State machine actions */

/* Idle convo receives "Request Mate" event */
int action_s0_e0(convo_state_t* state)
{
  /* Run the mate algorithm here, respond with "YES" or "NO" */
  /* TODO */
  fipa_acl_message_t* reply = mc_AclReply(state->acl);
  mc_AclSetPerformative(reply, FIPA_AGREE);
  mc_AclSetContent(reply, "YES");
  mc_AclSend(reply);
  return 1;
}

/* Idle convo timed out */
int action_s0_e3(convo_state_t* state)
{
  return 1;
}

/* Idle, received gene. Replace current gene with new gene */
int action_s0_e4(convo_state_t* state)
{
  const char* content = mc_AclMessageGetContent(state->acl);
  char* str = strdup(content+5);
  char* tok;
  int i;
  /* The beginning part of the content should be "GENE ", followed by 20 double
   * numbers. */
  tok = strtok(str, " ");
  for(i = 0; i < 20; i++) {
    sscanf(tok, "%lf", &gene[i]);
    tok = strtok(NULL, " ");
  }
  return 1;
}

/* Requested mate, received affirmative. Now we must wait for genes. */
int action_s1_e1(convo_state_t* state)
{
  state->cur_state = STATE_WAIT_FOR_GENE;
  return 0;
}

/* Requested mate, received negative. Terminate convo. */
int action_s1_e2(convo_state_t* state)
{
  return 1;
}

/* Requested mate, timed out. Terminate convo. */
int action_s1_e3(convo_state_t* state)
{
  return 1;
}

/* Waiting for gene, timed out. */
int action_s2_e3(convo_state_t* state)
{
  return 1;
}

/* Waiting for gene, received gene. Produce children. */
int action_s2_e4(convo_state_t* state)
{
  /* TODO */
  return 1;
}

int action_handle_error(convo_state_t* state) 
{
  return -1;
}

int action_invoke_error(convo_state_t* state)
{
  fipa_acl_message_t* reply;
  reply = mc_AclReply(state->acl);
  mc_AclSetPerformative(reply, FIPA_FAILURE);
  mc_AclSetContent(reply, "ERROR");
  return mc_AclSend(reply);
}

