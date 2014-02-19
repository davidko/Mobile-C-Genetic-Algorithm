#ifndef _NEWSCAST_H_
#define _NEWSCAST_H_

#define HOSTLIST_SIZE 10
typedef struct newscasthostinfo_s
{
  char* hostname;
  int time;
  double fitness;
} newscasthostinfo_t;

extern double g_avg_fitness;

#ifdef __cplusplus
extern "C" {
#endif

void* newscastAgentFunc(stationary_agent_info_t* arg);
void updateHosts(const char* content, newscasthostinfo_t* hosts, int *num_hosts);
int host_cmp(newscasthostinfo_t* host1, newscasthostinfo_t* host2);
void filter_host_duplicates(newscasthostinfo_t* hosts, int *num_hosts);

#ifdef __cplusplus
}
#endif

#endif
