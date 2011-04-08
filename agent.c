#pragma package "/usr/local/ch/package/chmobilec"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mobilec.h>
#include "convo_state_machine.h"

#define MATCH_CMD(str, cmd) \
  if (!strncmp(str, cmd, strlen(cmd)))

int handleSetGene(const char* msgContent);
int handleProcreate(const char* msgContent);

int age = 0;

int mate_attempts = 0;

#define GENE_SIZE 20
double gene[GENE_SIZE];
double g_fitness;
convo_state_t* g_convo_state_head;

agent_info_t *g_agent_info_entries[20];
int g_num_agent_info_entries;

int main()
{
  int i;
  const void* data;
  convo_state_t* convo_iter;
  fipa_acl_message_t* message;
  int event;
  char buf[80];
  /* Get the saved variables */
  data = mc_AgentVariableRetrieve(
      mc_current_agent, "age", 0); 
  if(data == NULL) {
    age = 0;
  } else {
    age = *(const int*)data;
  }

  data = mc_AgentVariableRetrieve(
      mc_current_agent, "mate_attempts", 0);
  if(data == NULL) {
    mate_attempts = 0;
  } else {
    mate_attempts = *(const int*)data;
  }

  data = mc_AgentVariableRetrieve(
      mc_current_agent, "gene", 0);
  if(data == NULL) {
    /* Request a new gene */
    /* Request a gene from the master agent */
    message = mc_AclNew();
    mc_AclSetPerformative(message, FIPA_REQUEST);
    i = rand();
    sprintf(buf, "%d", i);
    mc_AclSetConversationID(message, buf);
    mc_AclSetSender(message, mc_agent_name, mc_agent_address);
    mc_AclAddReceiver(message, "master", mc_agent_address);
    mc_AclSetContent(message, "REQUEST_GENE");
    mc_AclSend(message);
    mc_AclDestroy(message);
  } else {
    memcpy(gene, data, sizeof(double)*GENE_SIZE);
    g_fitness = costFunction(gene);
  }

  g_num_agent_info_entries = 0;

  /* Get my fitness */
  init_convo_state_machine();

  /* Main state machine loop */
  while(1) {
    /* Main operation loop */
    /* If there are any ACL messages, handle them */
    message = mc_AclRetrieve(mc_current_agent);
    if(message) {
      /* Get the 'event' for the message */
      event = messageGetEvent(message);
      /* See if there is an existing convo for this message */
      for(convo_iter = g_convo_state_head; convo_iter != NULL; convo_iter = convo_iter->next)
      {
        if(!strcmp(convo_iter->convo_id, mc_AclGetConversationID(message))) {
          convo_iter->acl = message;
          break;
        }
      }
      if(convo_iter == NULL) {
        /* New Conversation */
        convo_iter = convo_state_new(mc_AclGetConversationID(message));
        convo_iter->acl = message;
        convo_iter->timeout = 60;
        insert_convo(&g_convo_state_head, convo_iter);
      } 
      
      if(event < 0) {
        printf("ERROR: Invalid event. Message was: %s\n",  mc_AclGetContent(message));
      }
      if(state_table[convo_iter->cur_state][event](convo_iter))
      {
        /* Destroy and remove the convo */
        remove_convo(&g_convo_state_head, convo_iter);
        convo_state_destroy(convo_iter);
      }
    } else {
      usleep(200000);
    }
    /* Process all conversations for timeouts, etc */
    for(convo_iter = g_convo_state_head; convo_iter != NULL; convo_iter = convo_iter->next)
    {
      if(time(NULL) - convo_iter->time_last_action > convo_iter->timeout) {
        state_table[convo_iter->cur_state][EVENT_TIMEOUT](convo_iter);
        remove_convo(&g_convo_state_head, convo_iter);
        convo_state_destroy(convo_iter);
      }
    }

    /* If there are no currently proceeding conversations, have a 10% chance to
     * find same mates */
    if(g_convo_state_head == NULL && (double)rand()/(double)RAND_MAX < 0.1) {
      printf("Get agents...\n");
      sprintf(buf, "%d", rand());
      convo_iter = convo_state_new(buf);
      convo_iter->timeout = 60;
      convo_iter->cur_state = STATE_WAIT_FOR_AGENT_LIST;
      insert_convo(&g_convo_state_head, convo_iter);
      message = mc_AclNew();
      mc_AclSetPerformative(message, FIPA_REQUEST);
      mc_AclSetConversationID(message, buf);
      mc_AclSetSender(message, mc_agent_name, mc_agent_address);
      mc_AclAddReceiver(message, "master", mc_agent_address);
      mc_AclSetContent(message, "REQUEST_AGENTS");
      mc_AclSend(message);
      mc_AclDestroy(message);
    }
  }
  return 0;
}

int messageGetEvent(fipa_acl_message_t* message)
{
  DEBUGMSG;
  const char* content = mc_AclGetContent(message);
  MATCH_CMD(content, "REQUEST_MATE") {
    return EVENT_REQUEST_MATE;
  } else
  MATCH_CMD(content, "AFFIRMATIVE") {
    return EVENT_AFFIRM;
  } else
  MATCH_CMD(content, "NEGATIVE") {
    return EVENT_REJECT;
  } else 
  MATCH_CMD(content, "GENE") {
    return EVENT_RECV_GENE;
  } else
  MATCH_CMD(content, "ERROR") {
    return EVENT_ERROR;
  } else
  MATCH_CMD(content, "AGENTS") {
    return EVENT_AGENT_LIST;
  } else
  MATCH_CMD(content, "REQUEST_FITNESS") {
    return EVENT_REQUEST_FITNESS;
  } else
  MATCH_CMD(content, "FITNESS") {
    return EVENT_RECEIVE_FITNESS;
  } else
  MATCH_CMD(content, "YES") {
    return EVENT_AFFIRM;
  } else
  MATCH_CMD(content, "NO") {
    return EVENT_REJECT;
  }
  return -1;
}

