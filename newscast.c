#include <stdio.h>
#include <string.h>
#include "server.h"
#include "newscast.h"
#include <dynstring.h>

double g_avg_fitness = 0; // Current average fitness of the local agency 

/* This binary agent is the newscast manager for the agency, communicating with
 * other newscast agents to maintain a network of online agencies. */
void* newscastAgentFunc(stationary_agent_info_t* arg)
{
  MCAgency_t agency = MC_AgentInfo_GetAgency(arg);
  fipa_acl_message_t* acl, *reply;
  MCAgent_t agent = MC_AgentInfo_GetAgent(arg);
  int count = 0;
  newscasthostinfo_t* hosts = (newscasthostinfo_t*)malloc(sizeof(newscasthostinfo_t)*HOSTLIST_SIZE*3);
  int numhosts = 0;
  /* If there is a seed host, get it */
  char* seed;
  seed = MC_AgentInfo_GetAgentArgs(arg);
  if(seed) {
    hosts[0].hostname = strdup(seed);
    hosts[0].time = time(NULL);
    numhosts++;
  }
  dynstring_t* content;
  char buf[128];
  char last_request_convo_id[80];
  char last_request_hostname[128];
  char *cptr;
  int i;
  int state = 0; // 0 -> Idle; 1 -> Wait_inform
  int timeout;
  int request_timeout = time(NULL) + 30;
  while(1) {
    switch(state) {
      case 0: // Idle state
        /* Check for incoming messages */
        acl = MC_AclRetrieve(agent);
        if(acl != NULL) {
          /* If we received a request, send an inform response */
          if(MC_AclGetPerformative(acl) == FIPA_REQUEST) {
            updateHosts(MC_AclGetContent(acl), hosts, &numhosts);
            content = dynstring_New();
            for(i = 0; i < numhosts; i++) {
              sprintf(buf, "%s %d %lf\n", hosts[i].hostname, hosts[i].time, hosts[i].fitness);
              dynstring_Append(content, buf);
            }
            reply = MC_AclReply(acl);
            MC_AclSetPerformative(reply, FIPA_INFORM);
            MC_AclSetContent(reply, content->message);
            sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
            MC_AclSetSender(reply, "newscast", buf);
            MC_AclSend(agency, reply);
            MC_AclDestroy(reply);
            dynstring_Destroy(content);
            /* Reset the timeout for sending a request */
            request_timeout = time(NULL) + 30;
          } else {
            /* Discard the message */
            MC_AclDestroy(acl);
          }
        } else {
          /* Check our timeout(s) */
          if(time(NULL) > request_timeout) {
            /* Send a request to a random host */
            if(numhosts == 0) {
              request_timeout += 30;
              break;
            }
            i = rand() % numhosts;
            acl = MC_AclNew();
            MC_AclSetPerformative(acl, FIPA_REQUEST);
            /* Set the receiver */
            strcpy(buf, hosts[i].hostname);
            strcpy(last_request_hostname, buf);
            cptr = strtok(buf, " ");
            printf("Sending message to %s...\n", cptr);
            MC_AclAddReceiver(acl, "newscast", cptr);
            content = dynstring_New();
            /* First, add ourself */
            sprintf(buf, "http://%s:%d/acc %d %lf\n", g_hostname, g_localport, time(), g_avg_fitness);
            dynstring_Append(content, buf);
            /* Now add every other entry in our list */
            for(i = 0; i < numhosts; i++) {
              sprintf(buf, "%s %d %lf\n", hosts[i].hostname, hosts[i].time, hosts[i].fitness);
              dynstring_Append(content, buf);
            }
            MC_AclSetContent(acl, content->message);
            sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
            MC_AclSetSender(acl, "newscast", buf);
            sprintf(last_request_convo_id, "%d", rand());
            MC_AclSetConversationID(acl, last_request_convo_id);
            MC_AclSend(agency, acl);
            MC_AclDestroy(acl);
            dynstring_Destroy(content);
            /* Set the timeout */
            timeout = time(NULL) + 30;
            state = 1;
          }
        }
        usleep(500000);
        break;
      case 1: // Waiting for inform
        /* Check incoming messages */
        acl = MC_AclRetrieve(agent);
        if(acl) {
          /* Received a message. Make sure the conversation id is correct. */
          if(MC_AclGetPerformative(acl) != FIPA_INFORM) {
            if(MC_AclGetPerformative(acl) == FIPA_REQUEST) {
              updateHosts(MC_AclGetContent(acl), hosts, &numhosts);
              content = dynstring_New();
              for(i = 0; i < numhosts; i++) {
                sprintf(buf, "%s %d %lf\n", hosts[i].hostname, hosts[i].time, hosts[i].fitness);
                dynstring_Append(content, buf);
              }
              reply = MC_AclReply(acl);
              MC_AclSetPerformative(reply, FIPA_INFORM);
              MC_AclSetContent(reply, content->message);
              sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
              MC_AclSetSender(reply, "newscast", buf);
              MC_AclSend(agency, reply);
              MC_AclDestroy(reply);
              dynstring_Destroy(content);
              /* Reset the timeout for sending a request */
              request_timeout = time(NULL) + 30;
            }
            MC_AclDestroy(acl);
          } else {
            if(strcmp(MC_AclGetConversationID(acl), last_request_convo_id)) {
              MC_AclDestroy(acl);
            } else {
              /* Received correct response. Add list of hosts to our list */
              updateHosts(MC_AclGetContent(acl), hosts, &numhosts);
              state = 0;
              request_timeout = time(NULL) + 30;
            }
          }
        } else {
          /* Check our timeout */
          if(time(NULL) > timeout) {
            state = 0;
            request_timeout = time(NULL) + 30;
            /* Remove the timed-out host from our list */
          }
        }
        usleep(500000);
        break;
    }

#if 0
    acl = MC_AclRetrieve(agent);
    if(acl == NULL) {
      if(count > 30) {
        count = 0;
        if(numhosts == 0) {
          continue;
        }
        /* Pick a random host and transact with them */
        i = rand() % numhosts;
        acl = MC_AclNew();
        MC_AclSetPerformative(acl, FIPA_INFORM);
        /* Set the receiver */
        strcpy(buf, hosts[i].hostname);
        cptr = strtok(buf, " ");
        MC_AclAddReceiver(acl, "newscast", cptr);
        content = dynstring_New();
        /* First, add ourself */
        sprintf(buf, "http://%s:%d %d\n", g_hostname, g_localport, time());
        dynstring_Append(content, buf);
        /* Now add every other entry in our list */
        for(i = 0; i < numhosts; i++) {
          sprintf(buf, "%s %d\n", hosts[i].hostname, hosts[i].time);
          dynstring_Append(content, buf);
        }
        MC_AclSetContent(acl, content->message);
        sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
        MC_AclSetSender(acl, "newscast", buf);
        MC_AclSend(agency, acl);
        MC_AclDestroy(acl);
        dynstring_Destroy(content);
        /* Send a request as well */
      }
      sleep(1);
      count++;
    } else {
      switch(MC_AclGetPerformative(acl)) {
        case FIPA_REQUEST:
          content = dynstring_New();
          for(i = 0; i < numhosts; i++) {
            sprintf(buf, "%s %d", hosts[i].hostname, hosts[i].time);
            dynstring_Append(content, buf);
          }
          reply = MC_AclReply(acl);
          MC_AclSetPerformative(reply, FIPA_INFORM);
          MC_AclSetContent(reply, content->message);
          sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
          MC_AclSetSender(reply, "newscast", buf);
          MC_AclSend(agency, reply);
          MC_AclDestroy(reply);
          dynstring_Destroy(content);
          break;
        case FIPA_INFORM:
          updateHosts(MC_AclGetContent(acl), hosts, &numhosts);
          break;
      }
    }
#endif
  }
}

/* Content format should be something like this:
<hostname:port> <time> <avg_fitness>
http://host.com:5050 3123124 -2.13
http://host2.com:5023 12312341 -3.54
*/
void updateHosts(const char* content, newscasthostinfo_t* hosts, int *num_hosts)
{
  char* buf = strdup(content);
  printf("content is %s\n", content);
  char* pch;
  char* saveptr;
  pch = strtok_r(buf, " \n", &saveptr);
  while(
      (pch != NULL) &&
      (strlen(pch) > 0)
      )
  {
    hosts[*num_hosts].hostname = strdup(pch);
    pch = strtok_r(NULL, " \n", &saveptr);
    sscanf(pch, "%d", &hosts[*num_hosts].time);
    pch = strtok_r(NULL, " \n", &saveptr);
    sscanf(pch, "%lf", &hosts[*num_hosts].fitness);
    pch = strtok_r(NULL, " \n", &saveptr);
    (*num_hosts)++;
  }
  /* Sort the list */
  qsort(hosts, *num_hosts, sizeof(newscasthostinfo_t), host_cmp);

  /* Remove duplicates */
  filter_host_duplicates(hosts, num_hosts);

  /* Trim the list */
  int i;
  if(*num_hosts > HOSTLIST_SIZE) {
    for(i = HOSTLIST_SIZE; i < *num_hosts; i++) {
      free(hosts[i].hostname);
    }
    *num_hosts = HOSTLIST_SIZE;
  }

  /* DEBUG Print the hosts */
  printf("Hosts updated:\n");
  for(i = 0; i < *num_hosts; i++) {
    printf("%s\n", hosts[i].hostname);
  }
}

int host_cmp(newscasthostinfo_t* host1, newscasthostinfo_t* host2)
{
  return host1->time - host2->time;
}

void filter_host_duplicates(newscasthostinfo_t* hosts, int *num_hosts)
{
  int i,j;
  int found = 0;
  for(i = 0; i < *num_hosts; i++)
  {
    /* Try and find a match later in the list */
    found = 0;
    for(j = i+1; j < *num_hosts; j++) {
      if(!strcmp(hosts[i].hostname, hosts[j].hostname)) {
        found = 1;
        break;
      }
    }
    if(found) {
      (*num_hosts)--;
      free(hosts[i].hostname);
      for(j = i; j < *num_hosts; j++) {
        hosts[j] = hosts[j+1];
      }
      i--;
    }
  }
}
