#include "convo_state_machine.h"

extern double gene[20]; /* Global agent gene */

int get_gene_from_content(double gene[20], const char* content);

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
  state_table[0][9] = action_s0_e9;
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
  DEBUGMSG;
  if(convo->acl) {
    mc_AclDestroy(convo->acl);
  }
  free(convo->convo_id);
  free(convo);
  DEBUGMSG;
}

int insert_convo(convo_state_t** head, convo_state_t* node)
{
  DEBUGMSG;
  /* Just stick it onto the head */
  convo_state_t* tmp;
  tmp = *head;
  *head = node;
  node->next = tmp;
  DEBUGMSG;
  return 0;
}

int remove_convo(convo_state_t** head, convo_state_t* node)
{
  DEBUGMSG;
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
  DEBUGMSG;
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
  DEBUGMSG;
  free(agent_info->name);
  free(agent_info);
  DEBUGMSG;
}

/* State machine actions */

/* Idle convo receives "Request Mate" event */
int action_s0_e0(convo_state_t* state)
{
  DEBUGMSG;
  /* Run the mate algorithm here, respond with "YES" or "NO" */
  const char* content;
  double fitness;
  int mate_flag;
  char *buf;
  char tmp[20];
  int i;
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
  mc_AclSetSender(reply, mc_agent_name, mc_agent_address);
  if(mate_flag) {
    mc_AclSetPerformative(reply, FIPA_AGREE);
    mc_AclSetContent(reply, "YES");
  } else {
    mc_AclSetPerformative(reply, FIPA_CANCEL);
    mc_AclSetContent(reply, "NO");
  }
  mc_AclSend(reply);
  mc_AclDestroy(reply);

  /* If replied with "YES", also send the gene */
  if(mate_flag) {
    reply = mc_AclReply(state->acl);
    mc_AclSetSender(reply, mc_agent_name, mc_agent_address);
    mc_AclSetPerformative(reply, FIPA_INFORM);
    buf = malloc(sizeof(char) * 400);
    buf[0] = '\0';
    sprintf(buf, "GENE ");
    for(i = 0; i < 20; i++) {
      sprintf(tmp, "%lf ", gene[i]); 
      strcat(buf, tmp);
    }
    mc_AclSetContent(reply, buf);
    free(buf);
    mc_AclSend(reply);
    mc_AclDestroy(reply);
  }
  DEBUGMSG;
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
  g_fitness = costFunction(gene);
  printf("%s fitness is %lf\n", mc_agent_name, g_fitness);
  free(str);
  DEBUGMSG;
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
  DEBUGMSG;
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
  printf("Received fitness string: %s:%s -> %lf\n", name, mc_AclGetContent(state->acl), fitness);
  free(name);
  free(address);
  DEBUGMSG;
  return 1;
}

/* Got a request to terminate. Just terminate */
int action_s0_e9(convo_state_t* state)
{
  exit(0);
}

/* Requested mate, received affirmative. Now we must wait for genes. */
int action_s1_e1(convo_state_t* state)
{
  DEBUGMSG;
  state->cur_state = STATE_WAIT_FOR_GENE;
  DEBUGMSG;
  return 0;
}

/* Requested mate, received negative. Terminate convo. */
int action_s1_e2(convo_state_t* state)
{
  DEBUGMSG;
  g_num_rejects++;
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
  printf("Produce children!\n");
  /* Construct our hybrid gene */
  char* content = strdup(mc_AclGetContent(state->acl));
  int cutpoint = rand() % 20;
  double newgene[20];
  double othergene[20];
  int i;
  get_gene_from_content(othergene, content);
  for(i = 0; i < cutpoint; i++) {
    newgene[i] = gene[i];
  }
  for(i = cutpoint; i < 20; i++) {
    newgene[i] = othergene[i];
  }

  fipa_acl_message_t* message = mc_AclNew();
  mc_AclSetPerformative(message, FIPA_REQUEST);
  char buf[800];
  char tmp[40];
  sprintf(buf, "%d", rand());
  mc_AclSetConversationID(message, buf);
  mc_AclSetSender(message, mc_agent_name, mc_agent_address);
  mc_AclAddReceiver(message, "master", mc_agent_address);
  sprintf(buf, "REQUEST_CHILD ");
  for(i = 0; i < 20; i++) {
    sprintf(tmp, "%lf ", newgene[i]);
    strcat(buf, tmp);
  }
  mc_AclSetContent(message, buf);
  mc_AclSend(message);
  mc_AclDestroy(message);
  free(content);
  printf("Done producing children.\n");
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
    /* Reset our original list of fitnesses */
    for(i = 0; i < g_num_agent_info_entries; i++) {
      agent_info_destroy(g_agent_info_entries[i]);
    }
    g_num_agent_info_entries = 0;
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
      agent_name = strtok(NULL, " ");
    }
    free(content);

    /* Set up a state to begin mating process in some time */
    convo_state_t* state = convo_state_new("meh");
    state->cur_state = STATE_WAIT_FOR_INIT_MATE;
    state->timeout = AGENT_RESPONSE_WAIT_TIME;
    insert_convo(&g_convo_state_head, state);
  }
  DEBUGMSG;
  return 1;
}

/* Waiting period before mate selection has passed. Now we must select from
 * possible mates */
int action_s4_e3(convo_state_t* state)
{
  DEBUGMSG;
  int i, j;
  /* Propose to the top 5 or half of the length of the number of agents,
   * whichever is less. */
  int num_to_propose_to = 
      g_num_agent_info_entries / 2 > 5 ? 5 : (g_num_agent_info_entries/2);
  if(num_to_propose_to == 0) {
    return 1;
  }
  /* Sort the list */
  qsort(
      g_agent_info_entries, 
      g_num_agent_info_entries, 
      sizeof(agent_info_t*),
      compare_agent_info);
  /* Send messages to the top number requesting to mate */
  j = g_num_agent_info_entries - 1;
  fipa_acl_message_t* acl;
  for(i = 0; i < num_to_propose_to; i++ ) {
    init_mate_proposal(g_agent_info_entries[j]->name);
    j--;
  }
  DEBUGMSG;
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
  mc_AclSend(reply);
  mc_AclDestroy(reply);
  DEBUGMSG;
  return 0;
}

int compare_agent_info(const void* _a, const void* _b)
{
  agent_info_t* a = *(agent_info_t**)_a;
  agent_info_t* b = *(agent_info_t**)_b;

  if(a->fitness < b->fitness) {
    return -1;
  } else if ( a->fitness > b->fitness ) {
    return 1;
  } else {
    return 0;
  }
}

void init_mate_proposal(const char* name)
{
  DEBUGMSG;
  char buf[400];
  printf("Init mate to %s\n", name);
  sprintf(buf, "%d", rand());
  convo_state_t* convo;
  convo = convo_state_new(buf);
  convo->cur_state = STATE_REQUESTED_MATE;
  convo->timeout = 5;
  insert_convo(&g_convo_state_head, convo);
  fipa_acl_message_t* msg;
  msg = mc_AclNew();
  mc_AclSetSender(msg, mc_agent_name, mc_agent_address);
  mc_AclAddReceiver(msg, name, mc_agent_address);
  mc_AclSetPerformative(msg, FIPA_REQUEST);
  mc_AclSetConversationID(msg, buf);
  sprintf(buf, "REQUEST_MATE %lf", g_fitness);
  mc_AclSetContent(msg, buf);
  mc_AclSend(msg);
  mc_AclDestroy(msg);
  DEBUGMSG;
}

int get_gene_from_content(double gene[20], const char* content) {
  char *str = strdup(content);
  char *sp = str;
  char *saveptr;
  char *tmp;
  int i = 0;
  if(!strncmp(str, "GENE ", 5)) { 
    sp += 5;
  }
  tmp = strtok_r(sp, " \t", &saveptr);
  while(i < 20) {
    if(tmp == NULL) {
      free(str);
      return -1;
    }
    sscanf(tmp, "%lf", &gene[i]);
    tmp = strtok_r(NULL, " \t", &saveptr);
    i++;
  }
  free(str);
  return 0;
}
