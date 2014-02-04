#include "convo_state_machine.h"

extern double gene[GENE_SIZE]; /* Global agent gene */

int get_gene_from_content(double gene[GENE_SIZE], const char* content);

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
  state_table[1][9] = action_s1_e9;
  state_table[2][3] = action_s2_e3;
  state_table[2][4] = action_s2_e4;
  state_table[2][9] = action_s2_e9;
  state_table[3][6] = action_s3_e6;
  state_table[3][9] = action_s3_e9;
  state_table[4][3] = action_s4_e3;
  state_table[4][9] = action_s4_e9;
  for(i = 0; i < STATE_MAX; i++) {
    state_table[i][EVENT_ERROR] = action_handle_error;
  }
}

convo_state_t* convo_state_new(const char* convo_id)
{
  convo_state_t* convo;
  convo = (convo_state_t*)mymalloc(sizeof(convo_state_t), __FILE__, __LINE__);
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
  agent_info_t* info = (agent_info_t*)mymalloc(sizeof(agent_info_t), __FILE__, __LINE__);
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

void terminate(void)
{
  int i;
  /* End all conversations */
  convo_state_t* iter, *next;
  while(iter != NULL) {
    next = iter->next;
    convo_state_destroy(iter);
    iter = next;
  }
  /* Destroy all agent_info entries */
  for(i = 0; i < g_num_agent_info_entries; i++) {
    agent_info_destroy(g_agent_info_entries[i]);
  }
  free(g_agent_info_entries);
}

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
  mc_AclSetSender(reply, mc_agent_name, "localhost");
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
    mc_AclSetSender(reply, mc_agent_name, "localhost");
    mc_AclSetPerformative(reply, FIPA_INFORM);
    buf = mymalloc(GENE_SIZE*20*2, __FILE__, __LINE__);
    buf[0] = '\0';
    sprintf(buf, "GENE ");
    for(i = 0; i < GENE_SIZE; i++) {
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
  for(i = 0; i < GENE_SIZE; i++) {
    sscanf(tok, "%lf", &gene[i]);
    tok = strtok(NULL, " ");
  }
  g_fitness = costFunction(gene);
  mc_AgentVariableSave(mc_current_agent, "g_fitness");
  /* Publish the fitness */
  mc_AgentDataShare_Add(mc_current_agent, "fitness", &g_fitness, sizeof(g_fitness));
/*
  printf("Agent received genes of fitness %le ", g_fitness);
  for(i = 0; i < GENE_SIZE; i++) {
    printf("%lf ", gene[i]);
  }
  printf("\n");
*/
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
  mc_AclSetSender(reply, mc_agent_name, "localhost");
  mc_AclSetPerformative(reply, FIPA_INFORM);
  sprintf(buf, "FITNESS %le", g_fitness);
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
  free(name);
  free(address);
  DEBUGMSG;
  return 1;
}

/* Got a request to terminate. Just terminate */
int action_s0_e9(convo_state_t* state)
{
  terminate();
  return -2;
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

/* Got a request to terminate. Just terminate */
int action_s1_e9(convo_state_t* state)
{
  terminate();
  return -2;
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
  /* Construct our hybrid gene */
  char* content = strdup(mc_AclGetContent(state->acl));
  int cutpoint = rand() % GENE_SIZE;
  double newgene[GENE_SIZE];
  double othergene[GENE_SIZE];
  int i;
  get_gene_from_content(othergene, content);
  for(i = 0; i < cutpoint; i++) {
    newgene[i] = gene[i];
  }
  for(i = cutpoint; i < GENE_SIZE; i++) {
    newgene[i] = othergene[i];
  }

  fipa_acl_message_t* message = mc_AclNew();
  mc_AclSetPerformative(message, FIPA_REQUEST);
  char *buf;
  buf = mymalloc(20 * GENE_SIZE*2, __FILE__, __LINE__);
  char tmp[40];
  sprintf(buf, "%d", rand());
  mc_AclSetConversationID(message, buf);
  mc_AclSetSender(message, mc_agent_name, "localhost");
  mc_AclAddReceiver(message, "master", NULL);
  sprintf(buf, "REQUEST_CHILD ");
  for(i = 0; i < GENE_SIZE; i++) {
    sprintf(tmp, "%lf ", newgene[i]);
    strcat(buf, tmp);
  }
  mc_AclSetContent(message, buf);
  mc_AclSend(message);
  mc_AclDestroy(message);
  free(content);
  free(buf);
  return 1;
}

/* Got a request to terminate. Just terminate */
int action_s2_e9(convo_state_t* state)
{
  terminate();
  return -2;
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
  char** agent_names;
  content = strdup(mc_AclGetContent(state->acl));
  /* If there are less than 10 agents on the server, just terminate the convo. */
  sscanf(content, "%*s %d", &num_agents);
  if(num_agents < 10) {
    free(content);
    return 1;
  } else {
    agent_names = (char**)mymalloc((num_agents*2)*sizeof(char*), __FILE__, __LINE__);
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
    for(i = 0; agent_name != NULL && i < num_agents; i++) {
      agent_names[i] = agent_name;
      agent_name = strtok(NULL, " ");
    }
    /* Form and send the message */
    fipa_acl_message_t* message;
    int offset = rand();
    for(i = 0; i < 8 ; i++) {
      agent_name = agent_names[(offset+i)%num_agents];
      if(agent_name == NULL) {continue;}
      sprintf(buf, "%d", rand());
      message = mc_AclNew();
      mc_AclSetSender(message, mc_agent_name, "localhost");
      mc_AclSetPerformative(message, FIPA_REQUEST);
      mc_AclAddReceiver(message, agent_name, NULL);
      mc_AclSetConversationID(message, buf);
      mc_AclSetContent(message, "REQUEST_FITNESS");
      mc_AclSend(message);
      mc_AclDestroy(message);
    }
    free(content);
    free(agent_names);

    /* Set up a state to begin mating process in some time */
    convo_state_t* state = convo_state_new("meh");
    state->cur_state = STATE_WAIT_FOR_INIT_MATE;
    state->timeout = 30;
    insert_convo(&g_convo_state_head, state);
  }
  DEBUGMSG;
  return 1;
}

/* Got a request to terminate. Just terminate */
int action_s3_e9(convo_state_t* state)
{
  terminate();
  return -2;
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
  /* Lock the qsort lock */
  mc_MutexLock(875);
  /* Sort the list */
  qsort(
      g_agent_info_entries, 
      g_num_agent_info_entries, 
      sizeof(agent_info_t*),
      compare_agent_info);
  /* Unlock the qsort lock */
  mc_MutexUnlock(875);
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

/* Got a request to terminate. Just terminate */
int action_s4_e9(convo_state_t* state)
{
  terminate();
  return -2;
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
  if(state->acl != NULL) {
    reply = mc_AclReply(state->acl);
    mc_AclSetPerformative(reply, FIPA_FAILURE);
    mc_AclSetSender(reply, mc_agent_name, "localhost");
    mc_AclSetContent(reply, "ERROR");
    mc_AclSend(reply);
    mc_AclDestroy(reply);
  }
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
  sprintf(buf, "%d", rand());
  convo_state_t* convo;
  convo = convo_state_new(buf);
  convo->cur_state = STATE_REQUESTED_MATE;
  convo->timeout = AGENT_RESPONSE_WAIT_TIME;
  insert_convo(&g_convo_state_head, convo);
  fipa_acl_message_t* msg;
  msg = mc_AclNew();
  mc_AclSetSender(msg, mc_agent_name, "localhost");
  mc_AclAddReceiver(msg, name, NULL);
  mc_AclSetPerformative(msg, FIPA_REQUEST);
  mc_AclSetConversationID(msg, buf);
  sprintf(buf, "REQUEST_MATE %le", g_fitness);
  mc_AclSetContent(msg, buf);
  mc_AclSend(msg);
  mc_AclDestroy(msg);
  DEBUGMSG;
}

int get_gene_from_content(double gene[GENE_SIZE], const char* content) {
  char *str = strdup(content);
  char *sp = str;
  char *saveptr;
  char *tmp;
  int i = 0;
  if(!strncmp(str, "GENE ", 5)) { 
    sp += 5;
  }
  tmp = strtok_r(sp, " \t", &saveptr);
  while(i < GENE_SIZE) {
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
