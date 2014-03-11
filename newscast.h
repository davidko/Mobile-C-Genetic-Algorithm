#ifndef _NEWSCAST_H_
#define _NEWSCAST_H_

#define HOSTLIST_SIZE 10
class NewscastHostInfo
{
  public:
    NewscastHostInfo(const char *hostname, int time, double fitness);
    NewscastHostInfo(string hostname, int time, double fitness);

    void hostname(const char* hostname);
    void hostname(string hostname);
    string hostname();
    const char* hostname_cstr();

    int time();
    void time(int);

    double fitness();
    void fitness(double fitness);

    // Overload some operators for sorting
    inline bool operator==(const NewscastHostInfo &l, const NewscastHostInfo &r)
    {return l.time() == r.time();}
    inline bool operator!=(const NewscastHostInfo &l, const NewscastHostInfo &r)
    {return !operator==(l, r);}
    inline bool operator<(const NewscastHostInfo &l, const NewscastHostInfo &r)
    {return l.time() < r.time();}
    inline bool operator> (const NewscastHostInfo& lhs, const NewscastHostInfo& rhs)
    {return  operator< (rhs,lhs);}
    inline bool operator<=(const NewscastHostInfo& lhs, const NewscastHostInfo& rhs)
    {return !operator> (lhs,rhs);}
    inline bool operator>=(const NewscastHostInfo& lhs, const NewscastHostInfo& rhs)
    {return !operator< (lhs,rhs);}

  private:
    string hostname_;
    int time_;
    double fitness_;
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
