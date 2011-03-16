#pragma package "/usr/local/ch/package/chmobilec"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mobilec.h>

int handleSetGene(const char* msgContent);
int handleProcreate(const char* msgContent);

int age = 0;

int mate_attempts = 0;

double gene[20];

int main()
{
  int i;
  const void* data;
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
  return 0;
  while(1) {
    /* Main operation loop */
    /* If there are any ACL messages, handle them */
    printf("Retrieving message...\n");
    message = mc_AclRetrieve(mc_current_agent);
    if(message) {
      printf("Got a message.\n");
      printf("content: %s\n", mc_AclGetContent(message));
      handleAclMessage(message);
      continue;
    } else {
      usleep(200000);
    }
  }
}

int handleAclMessage(fipa_acl_message_t* message) 
{
#define MATCH_CMD(str, cmd) \
  if (!strncmp(str, cmd, strlen(cmd)))

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
