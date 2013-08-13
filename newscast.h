#ifndef _NEWSCAST_H_
#define _NEWSCAST_H_

#define HOSTLIST_SIZE 10
typedef struct newscasthostinfo_s
{
  char* hostname;
  int time;
} newscasthostinfo_t;

void updateHosts(const char* content, newscasthostinfo_t* hosts, int *num_hosts);
int host_cmp(newscasthostinfo_t* host1, newscasthostinfo_t* host2);
void filter_host_duplicates(newscasthostinfo_t* hosts, int *num_hosts);

#endif
