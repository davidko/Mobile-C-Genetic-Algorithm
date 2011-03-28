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

double gene[20];

int main()
{
  int i;
  const void* data;
  convo_state_t* convo_state = NULL;
  convo_state_t* convo_iter;
  int event;
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

  fipa_acl_message_t* message;
  /* Request a gene from the master agent */
  message = mc_AclNew();
  mc_AclSetPerformative(message, FIPA_REQUEST);
  mc_AclSetSender(message, mc_agent_name, mc_agent_address);
  mc_AclAddReceiver(message, "master", mc_agent_address);
  mc_AclSetContent(message, "REQUEST_GENE");
  mc_AclSend(message);
  mc_AclDestroy(message);
  message = mc_AclWaitRetrieve(mc_current_agent);
  printf("%s\n", mc_AclGetContent(message));
  init_convo_state_machine();
  while(1) {
    /* Main operation loop */
    /* If there are any ACL messages, handle them */
    printf("Retrieving message...\n");
    message = mc_AclRetrieve(mc_current_agent);
    if(message) {
      printf("Got a message.\n");
      printf("content: %s\n", mc_AclGetContent(message));
      /* Get the 'event' for the message */
      event = messageGetEvent(message);
      /* See if there is an existing convo for this message */
      for(convo_iter = convo_state; convo_iter != NULL; convo_iter = convo_iter->next)
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
        insert_convo(convo_state, convo_iter);
      } 

      if(state_table[convo_iter->cur_state][event](convo_iter))
      {
        /* Destroy and remove the convo */
        remove_convo(convo_state, convo_iter);
        convo_state_destroy(convo_iter);
      }
    } else {
      usleep(200000);
    }
    /* Process all conversations for timeouts, etc */
    for(convo_iter = convo_state; convo_iter != NULL; convo_iter = convo_iter->next)
    {
      if(time(NULL) - convo_iter->time_last_action > convo_iter->timeout) {
        state_table[convo_iter->cur_state][EVENT_TIMEOUT](convo_iter);
        remove_convo(convo_state, convo_iter);
        convo_state_destroy(convo_iter);
      }
    }
  }
  return 0;
}

int messageGetEvent(fipa_acl_message_t* message)
{
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
  }
  return -1;
}

int handleAclMessage(fipa_acl_message_t* message) 
{

  const char* content;
  content = mc_AclGetContent(message);
  if(mc_AclGetPerformative(message) == FIPA_INFORM) {
    MATCH_CMD(content, "SET_GENE ") {
      return handleSetGene(content);
    } else 
    MATCH_CMD(content, "INIT_PROCREATE ") {
      return handleProcreate(content);
    }
  }
  return 0;
}

int handleSetGene(const char* msgContent)
{
  printf("Handle Gene: %s\n", msgContent);
  return 0;
}

int handleProcreate(const char* msgContent)
{ 
  return 0;
}
