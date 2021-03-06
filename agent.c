#pragma package "/usr/local/ch/package/chmobilec"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mobilec.h>
#include "convo_state_machine.h"

#define MATCH_CMD(str, cmd) \
  if (!strncmp(str, cmd, strlen(cmd)))

#define CONVO_TIMEOUT 180

#define TMPFILE_MUTEX 324

int age = 0;

int mate_attempts = 0;

double gene[GENE_SIZE];
double g_fitness;
convo_state_t* g_convo_state_head;

agent_info_t **g_agent_info_entries;
int g_num_agent_info_entries;

int g_num_rejects;

void* mymalloc(size_t size, const char* file, int line) {
  printf("MALLOC %d at %s:%d\n", size, file, line);
  return malloc(size);
}

char *g_logfile;
void saveLogFile();

void saveLogFile()
{
  /* Print stats to logfile */
  int rc;
  int i;
  size_t size;
  rc = mc_AgentDataShare_Retrieve(mc_current_agent, "logfile_dir", &g_logfile, &size);
  if(rc) {
    fprintf(stderr, "Error retrieving logfile directory.\n");
    exit(-1);
  }
  FILE *fp;
  char tmpfilename[128];
  sprintf(tmpfilename, "%s/%d_%s", g_logfile, time(NULL), mc_agent_name);
  fp = fopen(tmpfilename, "w");
  if(fp != NULL) {
    for(i = 0; i < GENE_SIZE; i++) {
      fprintf(fp, "%lf\n", gene[i]);
    }
    fprintf(fp, "%lf\n", g_fitness);
    fclose(fp);
  }
}

int main()
{
  int rc;
  int i;
  const void* data;
  convo_state_t* convo_iter;
  convo_state_t* convo_tmp;
  fipa_acl_message_t* message;
  int event;
  char buf[80];
  g_agent_info_entries = (agent_info_t**)mymalloc(sizeof(agent_info_t*)*80, __FILE__, __LINE__);
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
    mc_AclSetSender(message, mc_agent_name, "localhost");
    mc_AclAddReceiver(message, "master", NULL);
    mc_AclSetContent(message, "REQUEST_GENE");
    mc_AclSend(message);
    mc_AclDestroy(message);
  } else {
    memcpy(gene, data, sizeof(double)*GENE_SIZE);
    g_fitness = costFunction(gene);
    mc_AgentVariableSave(mc_current_agent, "g_fitness");
    saveLogFile();
  }

  g_num_agent_info_entries = 0;
  g_num_rejects = 0;

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
          if(convo_iter->acl) {
            mc_AclDestroy(convo_iter->acl);
          }
          convo_iter->acl = message;
          break;
        }
      }
      if(convo_iter == NULL) {
        /* New Conversation */
        convo_iter = convo_state_new(mc_AclGetConversationID(message));
        convo_iter->acl = message;
        convo_iter->timeout = CONVO_TIMEOUT;
        insert_convo(&g_convo_state_head, convo_iter);
      } 
      
      if(event < 0) {
        printf("ERROR: Invalid event. Message was: %s\n",  mc_AclGetContent(message));
      }
      rc = state_table[convo_iter->cur_state][event](convo_iter); 
      if(rc == -2)
      {
        return 0;
      } else if (rc)
      {
        /* Destroy and remove the convo */
        remove_convo(&g_convo_state_head, convo_iter);
        convo_state_destroy(convo_iter);
      }
    } else {
      usleep(200000);
    }
    /* Process all conversations for timeouts, etc */
    convo_iter = g_convo_state_head;
    while(convo_iter != NULL)
    {
      if(time(NULL) - convo_iter->time_last_action > convo_iter->timeout) {
        state_table[convo_iter->cur_state][EVENT_TIMEOUT](convo_iter);
        remove_convo(&g_convo_state_head, convo_iter);
        convo_tmp = convo_iter->next;
        convo_state_destroy(convo_iter);
        convo_iter = convo_tmp;
      } else {
        convo_iter = convo_iter->next;
      }
    }

    /* If there are no currently proceeding conversations, have a 10% chance to
     * find same mates */
    if(g_convo_state_head == NULL && (double)rand()/(double)RAND_MAX < 0.1) {
      sprintf(buf, "%d", rand());
      convo_iter = convo_state_new(buf);
      convo_iter->timeout = CONVO_TIMEOUT;
      convo_iter->cur_state = STATE_WAIT_FOR_AGENT_LIST;
      insert_convo(&g_convo_state_head, convo_iter);
      message = mc_AclNew();
      mc_AclSetPerformative(message, FIPA_REQUEST);
      mc_AclSetConversationID(message, buf);
      mc_AclSetSender(message, mc_agent_name, "localhost");
      mc_AclAddReceiver(message, "master", NULL);
      mc_AclSetContent(message, "REQUEST_AGENTS");
      mc_AclSend(message);
      mc_AclDestroy(message);
    }
    /* If there are no currently proceeding convos, have a 1% chance to migrate */
    if(g_convo_state_head == NULL && (double)rand()/(double)RAND_MAX < 0.01) {
      sprintf(buf, "%d", rand());
      convo_iter = convo_state_new(buf);
      convo_iter->timeout = CONVO_TIMEOUT;
      convo_iter->cur_state = STATE_WAIT_FOR_NEWSCAST_RESPONSE;
      insert_convo(&g_convo_state_head, convo_iter);
      message = mc_AclNew();
      mc_AclSetPerformative(message, FIPA_REQUEST);
      mc_AclSetConversationID(message, buf);
      mc_AclSetSender(message, mc_agent_name, "localhost");
      mc_AclAddReceiver(message, "newscast", NULL);
      mc_AclSetContent(message, "REQUEST_HOSTS");
      mc_AclSend(message);
      mc_AclDestroy(message);
      printf("%s requesting hosts from newscast.\n");
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
  } else
  MATCH_CMD(content, "TERMINATE") {
    printf("AGENT TERMINATE MESSAGE RECEIVED\n");
    return EVENT_TERMINATE;
  }
  return -1;
}

