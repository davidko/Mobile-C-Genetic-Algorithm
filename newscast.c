#include <stdio.h>
#include <string.h>
#include "server.h"
#include "newscast.h"
#include <dynstring.h>

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
  dynstring_t* content;
  char buf[128];
  int i;
  while(1) {
    acl = MC_AclRetrieve(agent);
    if(acl == NULL) {
      if(count > 30) {
        count = 0;
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
          break;
        case FIPA_INFORM:
          updateHosts(MC_AclGetContent(acl), hosts, &numhosts);
          break;
      }
    }
  }
}

/* Content format should be something like this:
http://host.com:5050 3123124
http://host2.com:5023 12312341
*/
void updateHosts(const char* content, newscasthostinfo_t* hosts, int *num_hosts)
{
  char* buf = strdup(content);
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
    strtok_r(NULL, " \n", &saveptr);
    (*num_hosts)++;
  }
  /* Sort the list */
  qsort(hosts, sizeof(newscasthostinfo_t), *num_hosts, host_cmp);

  /* Remove duplicates */
  filter_host_duplicates(hosts, num_hosts);

  /* Trim the list */
  if(*num_hosts > HOSTLIST_SIZE) {
    for(i = HOSTLIST_SIZE; i < *num_hosts; i++) {
      free(hosts[i].hostname);
    }
    *num_hosts = HOSTLIST_SIZE;
  }
}

int host_cmp(newscasthostinfo_t* host1, newscasthostinfo_t* host2)
{
  return host1.time - host2.time;
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
      *num_hosts--;
      free(hosts[i].hostname);
      for(j = i; j < *num_hosts; j++) {
        hosts[j] = hosts[j+1];
      }
      continue;
    }
  }
}
