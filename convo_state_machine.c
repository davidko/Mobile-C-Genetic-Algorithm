#include "convo_state_machine.h"

extern double gene[20]; /* Global agent gene */

int (*const state_table[STATE_MAX][EVENT_MAX]) (convo_state_t* state);

void init_convo_state_machine()
{
  int i, j;
  memset(state_table, 0, sizeof(state_table));
  /* Load 'invoke error' as default action for all events */
  for(i = 0; i < STATE_MAX; i++) {
    for(j = 0; j < EVENT_MAX; j++) {
      state_table[i][j] = action_invoke_error;
    }
  }

  state_table[0][0] = action_s0_e0;
  state_table[0][3] = action_s0_e3;
  state_table[0][4] = action_s0_e4;
  state_table[0][7] = action_s0_e7;
  state_table[0][8] = action_s0_e8;
  state_table[1][1] = action_s1_e1;
  state_table[1][2] = action_s1_e2;
  state_table[1][3] = action_s1_e3;
  state_table[2][3] = action_s2_e3;
  state_table[2][4] = action_s2_e4;
  state_table[3][6] = action_s3_e6;
  state_table[4][3] = action_s4_e3;
  for(i = 0; i < STATE_MAX; i++) {
    state_table[i][EVENT_ERROR] = action_handle_error;
  }
}

convo_state_t* convo_state_new(const char* convo_id)
{
  convo_state_t* convo;
  convo = (convo_state_t*)malloc(sizeof(convo_state_t));
  convo->convo_id = strdup(convo_id);
  convo->cur_state = 0;
  convo->time_last_action = time(NULL);
  convo->timeout = 0;
  convo->acl = NULL;
  return convo;
}

void convo_state_destroy(convo_state_t* convo)
{
  free(convo->convo_id);
  free(convo);
}

int insert_convo(convo_state_t** head, convo_state_t* node)
{
  /* Just stick it onto the head */
  convo_state_t* tmp;
  tmp = *head;
  *head = node;
  node->next = tmp;
  return 0;
}

int remove_convo(convo_state_t** head, convo_state_t* node)
{
  convo_state_t* iter;
  /* First see if the head matches */
  if(node == *head) {
    *head = (*head)->next;
    return 0;
  }
  /* If it's not the head, traverse the list */
  iter = *head;
  while(iter->next != NULL) {
    if(iter->next == node) {
      iter->next = iter->next->next;
      return 0;
    }
    iter = iter->next;
  }
  return 1;
}

agent_info_t* agent_info_new(const char* name, double fitness)
{
  agent_info_t* info = (agent_info_t*)malloc(sizeof(agent_info_t));
  info->name = strdup(name);
  info->fitness = fitness;
  return info;
}

void agent_info_destroy(agent_info_t* agent_info)
{
  free(agent_info->name);
  free(agent_info);
}

/* State machine actions */

/* Idle convo receives "Request Mate" event */
int action_s0_e0(convo_state_t* state)
{
  DEBUGMSG;
  /* Run the mate algorithm here, respond with "YES" or "NO" */
  /* TODO */
  const char* content;
  double fitness;
  int mate_flag;
  content = mc_AclGetContent(state->acl);
  sscanf(content, "%*s %lf", &fitness);
  if(g_fitness > fitness) {
    /* We are more fit than the one asking us. Not likely to mate */
    if((double)rand()/(double)RAND_MAX < PROBABILITY_MATE_INFERIOR) {
      mate_flag = 1;
    } else {
      mate_flag = 0;
    }
  } else {
    /* We are less fit than the asker. Likely to mate */
    if((double)rand()/(double)RAND_MAX < PROBABILITY_MATE_SUPERIOR) {
      mate_flag = 1;
    } else {
      mate_flag = 0;
    }
  }
  fipa_acl_message_t* reply = mc_AclReply(state->acl);
  mc_AclSetPerformative(reply, FIPA_AGREE);
  if(mate_flag) {
    mc_AclSetContent(reply, "YES");
  } else {
    mc_AclSetContent(reply, "NO");
  }
  mc_AclSend(reply);
  return 1;
}

/* Idle convo timed out */
int action_s0_e3(convo_state_t* state)
{
  DEBUGMSG;
  return 1;
}

/* Idle, received gene. Replace current gene with new gene */
int action_s0_e4(convo_state_t* state)
{
  DEBUGMSG;
  const char* content = mc_AclGetContent(state->acl);
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
  printf("New gene:\n");
  for(i = 0; i < 20; i++) {
    printf("%lf ", gene[i]);
  }
  printf("\n");
  return 1;
}

/* Idle, got a request for my fitness */
int action_s0_e7(convo_state_t* state)
{
  DEBUGMSG;
  char buf[160];
  fipa_acl_message_t* reply = mc_AclReply(state->acl);
  mc_AclSetSender(reply, mc_agent_name, mc_agent_address);
  mc_AclSetPerformative(reply, FIPA_INFORM);
  sprintf(buf, "FITNESS %lf", g_fitness);
  mc_AclSetContent(reply, buf);
  mc_AclSend(reply);
  mc_AclDestroy(reply);
  return 1;
}

/* Got another agent's fitness. Add it to our list */
int action_s0_e8(convo_state_t* state)
{
  DEBUGMSG;
  char *name, *address;
  double fitness;
  sscanf(mc_AclGetContent(state->acl), "%*s %lf", &fitness);
  mc_AclGetSender(state->acl, &name, &address);
  g_agent_info_entries[g_num_agent_info_entries] = 
    agent_info_new(name, fitness);
  g_num_agent_info_entries++;
  free(name);
  free(address);
  return 1;
}

/* Requested mate, received affirmative. Now we must wait for genes. */
int action_s1_e1(convo_state_t* state)
{
  DEBUGMSG;
  state->cur_state = STATE_WAIT_FOR_GENE;
  return 0;
}

/* Requested mate, received negative. Terminate convo. */
int action_s1_e2(convo_state_t* state)
{
  DEBUGMSG;
  return 1;
}

/* Requested mate, timed out. Terminate convo. */
int action_s1_e3(convo_state_t* state)
{
  DEBUGMSG;
  return 1;
}

/* Waiting for gene, timed out. */
int action_s2_e3(convo_state_t* state)
{
  DEBUGMSG;
  return 1;
}

/* Waiting for gene, received gene. Produce children. */
int action_s2_e4(convo_state_t* state)
{
  DEBUGMSG;
  /* TODO */
  return 1;
}

/* Waiting for an agent list, received an agent list */
int action_s3_e6(convo_state_t* state)
{
  DEBUGMSG;
  int num_agents;
  char* content;
  char* agent_name;
  int i;
  char buf[80];
  content = strdup(mc_AclGetContent(state->acl));
  /* If there are less than 10 agents on the server, just terminate the convo. */
  sscanf(content, "%*s %d", &num_agents);
  if(num_agents < 5) {
    return 1;
  } else {
    /* Request the fitness of 10 agents */
    strtok(content, " ");
    /* Skip the first and second argument */
    strtok(NULL, " ");
    strtok(NULL, " ");
    agent_name = strtok(NULL, " ");
    /* Form and send the message */
    fipa_acl_message_t* message;
    for(i = 0; i < 5 && agent_name != NULL; i++) {
      sprintf(buf, "%d", rand());
      message = mc_AclNew();
      mc_AclSetSender(message, mc_agent_name, mc_agent_address);
      mc_AclSetPerformative(message, FIPA_REQUEST);
      mc_AclAddReceiver(message, agent_name, mc_agent_address);
      mc_AclSetConversationID(message, buf);
      mc_AclSetContent(message, "REQUEST_FITNESS");
      mc_AclSend(message);
      mc_AclDestroy(message);
    }
    free(content);

    /* Set up a state to begin mating process in some time */
    convo_state_t* state = convo_state_new("meh");
    state->cur_state = STATE_WAIT_FOR_INIT_MATE;
    state->timeout = AGENT_RESPONSE_WAIT_TIME;
    insert_convo(&g_convo_state_head, state);
  }
  return 1;
}

/* Waiting period before mate selection has passed. Now we must select from
 * possible mates */
int action_s4_e3(convo_state_t* state)
{
  DEBUGMSG;
  printf("Select mates now!\n");
  return 1;
}

int action_handle_error(convo_state_t* state) 
{
  DEBUGMSG;
  return -1;
}

int action_invoke_error(convo_state_t* state)
{
  DEBUGMSG;
  fipa_acl_message_t* reply;
  reply = mc_AclReply(state->acl);
  mc_AclSetPerformative(reply, FIPA_FAILURE);
  mc_AclSetSender(reply, mc_agent_name, mc_agent_address);
  mc_AclSetContent(reply, "ERROR");
  return mc_AclSend(reply);
}

