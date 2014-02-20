
#include <stdio.h>
#include <libmc.h>
#include <fipa_acl.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h>
#include <sstream>
#include <queue>
#include "gene.h"

#include "newscast.h"

#define MATCH_CMD(str, cmd) \
  if (!strncmp(str, cmd, strlen(cmd)))

#define AGENT_POPULATION 40
#define GENE_SIZE 120
#define NUM_GENERATIONS 7200

#define QSORT_MUTEX 875
#define TMPFILE_MUTEX 324

typedef struct AgentInfo_s {
  char* name;
  double fitness;
} AgentInfo_t;

int agentCallbackFunc(ChInterp_t interp, MCAgent_t agent, void* user_data);
EXPORTCH double cost_chdl(void* varg);
void* masterAgentFunc(stationary_agent_info_t* agent_info);
int composeSortedAgentList(MCAgency_t agency, AgentInfo_t **agentList, int *numAgents);
int compareAgentInfo(const void* _a, const void* _b);
void* cullAgentFunc(stationary_agent_info_t* agent_info);
int startAgent(MCAgency_t agency);

int handleInform(fipa_acl_message_t* acl);
int handleDefault(fipa_acl_message_t* acl);
int handleRequest(fipa_acl_message_t* acl);

pthread_mutex_t callback_lock;
pthread_cond_t callback_cond;
int callback_num_instances = 0;
#define CALLBACK_MAX_INSTANCES 2

/* When an agent requests a reproduction, it will send a message to the server
 * saying it wants to create a child with a certain gene. The server will then
 * store that gene in the following variable and create a generic child agent.
 * When the child agent initializes, it will ask the server for a gene, at
 * which time the server will provide the following gene, and unset the flag.
 * If an agent asks for a gene and the flag is not set, the server will create
 * a random gene. */

std::queue<Gene*> g_geneQueue;

double points[20][2];

int g_numsims = 0;

char g_logdir[128];

MCAgency_t agency;

stationary_agent_info_t* g_master_agent;

struct agent_info_s {
  char name[40];
  double fitness;
};

char* g_hostname;
int g_localport;

int main(int argc, char* argv[]) 
{
  int rc;

  if(argc != 3 && argc != 4) {
    printf("Command format: %s <local_hostname> <port> [newscast_seed]\n", argv[0]);
    printf("newscast_seed should be something like http://192.168.1.101:5050/acc\n");
    exit(0);
  }

  /* Prepare the log directory */
  time_t rawtime;
  struct tm* timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(g_logdir, sizeof(g_logdir), "logfiles_%F-%H%M.%S", timeinfo);
  rc = mkdir(g_logdir, 0755);
  if(rc) {
    fprintf(stderr, "Error creating logfile directory.\n");
    exit(-1);
  }

  g_hostname = new char[200];
  strcpy(g_hostname, argv[1]);
  int local_port;
  sscanf(argv[2], "%d", &local_port);
  g_localport = local_port;

  MCAgencyOptions_t options;
  int i;
  double *point = &(points[0][0]);
  MCAgent_t agent;
  MCAgent_t *agents;
  int num_agents;
  struct agent_info_s agent_info[100];
  srand(time(NULL));
  for(i = 0; i < 40; i++) {
    *point = (rand() % 100) - 50;
    point++;
  }

  pthread_mutex_init(&callback_lock, NULL);
  pthread_cond_init(&callback_cond, NULL);
  
  MC_InitializeAgencyOptions(&options);
  MC_SetThreadOff(&options, MC_THREAD_CP);
  agency = MC_Initialize(local_port, &options);
  if(agency == NULL) {
    fprintf(stderr, "Error starting agency.\n");
    exit(-1);
  }

  MC_AddAgentInitCallback(agency, agentCallbackFunc, NULL);

  /* Add qsort mutex */
  MC_SyncInit(agency, QSORT_MUTEX);
  MC_SyncInit(agency, TMPFILE_MUTEX);

  /* Start the newscast agent */
  if(argc == 4) {
    MC_AddStationaryAgent(agency, newscastAgentFunc, "newscast", argv[3]);
  } else {
    MC_AddStationaryAgent(agency, newscastAgentFunc, "newscast", NULL);
  }
  /* Start the master handler agent */
  MC_AddStationaryAgent(agency, masterAgentFunc, "master", NULL);
  /* Start the agent responsible for controlling the population */
  MC_AddStationaryAgent(agency, cullAgentFunc, "cullAgent", NULL);

  /* Start some agents */
  for(i = 0; i < AGENT_POPULATION; i++) {
    startAgent(agency);
    usleep(500000);
  }

  AgentInfo_t *agentList;
  FILE *logfile;
  char logfilename[128];
  sprintf(logfilename, "%s/logfile.txt", g_logdir);
  logfile = fopen(logfilename, "w");
  int j = 0;
  while(1) {
  /* Every five seconds or so, get a list of all the agents and kill enough
   * agents so that we have a standing population of 30 agents. */
    sleep(5);
    composeSortedAgentList(agency, &agentList, &num_agents);
    if (num_agents < 5) {
      continue;
    }	
    /* Calculate avg fitness */
    int i;
    double avgfitness = 0;
    for(i = 0; i < num_agents; i++) {
      avgfitness += agentList[i].fitness;
    }
    avgfitness /= (double)num_agents;
    g_avg_fitness = avgfitness;
    fprintf(logfile, "%d %d %lf %lf %lf %d\n", 
        j, num_agents, avgfitness, agentList[0].fitness, agentList[num_agents-1].fitness, g_numsims);
    fflush(logfile);
    free(agentList);
    j++;
    /*
    if(j == NUM_GENERATIONS) {
      exit(0);
    }
    */
  }

  MC_End(agency);
  return 0;
}

void* masterAgentFunc(stationary_agent_info_t* agent_info)
{
  g_master_agent = agent_info;
  int i;
  fipa_acl_message_t* acl;
  while(1) {
    acl = MC_AclWaitRetrieve(MC_AgentInfo_GetAgent(agent_info));
    switch(MC_AclGetPerformative(acl)) {
      case FIPA_REQUEST:
        handleRequest(acl);
        break;
      case FIPA_INFORM:
        handleInform(acl);
        break;
      default:
        handleDefault(acl);
        break;
    }
  }
}

/* This binary agent is responsible for culling agents from time to time to
 * keep the agent population in check. */
void* cullAgentFunc(stationary_agent_info_t* agent_info)
{
  MCAgency_t agency = MC_AgentInfo_GetAgency(agent_info);
  AgentInfo_t *agentList;
  int numAgents;
  int i;
  fipa_acl_message_t* msg;
  char buf[128];
  sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
  while(1) {
    sleep(2);
    MC_HaltAgency(agency);
    composeSortedAgentList(agency, &agentList, &numAgents);
    if(numAgents > AGENT_POPULATION) {
      /* Terminate the end of the list */
      for(i = AGENT_POPULATION; i < numAgents; i++) {
        printf("CULL %s\n", agentList[i].name);
        /* Send termination message to each of these agents */
        msg = MC_AclNew();
        MC_AclSetPerformative(msg, FIPA_INFORM);
        MC_AclAddReceiver(msg, (agentList[i]).name, buf);
        MC_AclSetContent(msg, "TERMINATE");
        MC_AclSetSender(msg, "master", buf);
        MC_AclSetConversationID(msg, "none");
        MC_AclSend(agency, msg);
        MC_AclDestroy(msg);
      }
    }
    for(i = 0; i < numAgents; i++) {
      free(agentList[i].name);
    }
    free(agentList);
    MC_ResumeAgency(agency);
  }
}

int composeSortedAgentList(MCAgency_t agency, AgentInfo_t **agentList, int *numAgents)
{
  MCAgent_t* agents;
  int num, i, j;
  char* name;
  double* fitness;
  size_t size;
  MC_AgentProcessingBegin(agency);
  MC_GetAllAgents(agency, &agents, &num);
  *agentList = (AgentInfo_t*)malloc(sizeof(AgentInfo_t)*(num+10));
  j = 0;
  for(i = 0; i < num; i++) {
    name = MC_GetAgentName(agents[i]);
    if(
        !strcmp(name, "master") || 
        !strcmp(name, "cullAgent") ||
        !strcmp(name, "newscast")) {
      free(name);
      continue;
    }
    MC_AgentDataShare_Retrieve(agents[i], "fitness", (void**)&fitness, &size);
    if(fitness == NULL) {
      continue;
    }
    (*agentList)[j].name = name;
    (*agentList)[j].fitness = *fitness;

    j++;
  }
  *numAgents = j;
  MC_AgentProcessingEnd(agency);

  /* Lets go ahead and sort the agent list here */
  if(*numAgents > 0) 
    qsort(*agentList, *numAgents, sizeof(AgentInfo_t),  compareAgentInfo);
  return 0;
}

int compareAgentInfo(const void* _a, const void* _b)
{
  AgentInfo_t *a, *b;
  a = (AgentInfo_t*)_a;
  b = (AgentInfo_t*)_b;
  if(a->fitness > b->fitness) {
    return 1;
  } else if (a->fitness < b->fitness) {
    return -1;
  } else {
    return 0;
  }
}

int startAgent(MCAgency_t agency)
{
  fipa_acl_message_t* acl;
  MCAgent_t agent;
  double gene[GENE_SIZE];
  /* Generate a random name */
  char name[80];
  char content[400];
  char buf[128];
  int i;
  sprintf(buf, "%s:%d", g_hostname, g_localport);
  sprintf(name, "agent%d", rand());
  agent = MC_ComposeAgentFromFile(
      name,
      buf,
      "IEL", 
      "agent.c",
      NULL,
      buf,
      0);

  printf("Starting agent...\n");  
  MC_AddAgent(agency, agent);
  return 0;
}

/* This callback function is called during the initialization step of each
 * incoming agent. We will add a c-space function to each interpreter that
 * the agent will be able to call. */
int agentCallbackFunc(ChInterp_t interp, MCAgent_t agent, void* user_data)
{
  Ch_DeclareFunc(interp, "double costFunction(double *x);", (ChFuncdl_t)cost_chdl);
  MC_AgentDataShare_Add(agent, "logfile_dir", g_logdir, strlen(g_logdir)+1);
  return 0; /* 0 for success error status */
}

EXPORTCH double cost_chdl(void* varg)
{
  pthread_mutex_lock(&callback_lock);
  while(callback_num_instances > CALLBACK_MAX_INSTANCES) {
    pthread_cond_wait(&callback_cond, &callback_lock);
  }
  callback_num_instances++;
  pthread_mutex_unlock(&callback_lock);

  static double lowest_gene = 0;
  double retval;
  double *gene;
  ChInterp_t interp;
  ChVaList_t ap;

  Ch_VaStart(interp, ap, varg);
  gene = Ch_VaArg(interp, ap, double*);
  int i, j;
  retval = 0;
  double value = 0, term;
#if 0
  for(i = 0; i < 20; i++) {
    retval += abs(gene[i]);
  }
#endif
  /* Create temporary file with genes */
  char tmpfilename[80];
  strcpy(tmpfilename, "tmpgenefile.XXXXXX");
  int fd = mkstemp(tmpfilename);
  FILE* fp = fdopen(fd, "w");
  if(fp == NULL) {
    fprintf(stderr, "WARNING: Could not open temporary file: %s\n", tmpfilename);
    return 0;
  }
  for(i = 0; i < GENE_SIZE; i++) {
    fprintf(fp, "%.0lf\n", gene[i]);
  }
  fclose(fp);
  /* Run the simulation program */
  char outputfilename[80];
  strcpy(outputfilename, tmpfilename);
  strcat(outputfilename, ".output");
  char buf[200];
  sprintf(buf, "LD_LIBRARY_PATH=/usr/lib/panda3d ../MobotGA/main --disable-graphics --load-coefs %s > %s",
      tmpfilename, outputfilename);
  system(buf);
  g_numsims++;
  fp = fopen(outputfilename, "r");
  if(fp == NULL) {
    fprintf(stderr, "WARNING: Could not open temporary file: %s\n", outputfilename);
    return 0;
  }
  double x, y, z;
  fscanf(fp, "%lf %lf %lf", &x, &y, &z);
  retval = -sqrt(x*x + y*y + z*z);
  fclose(fp);
  unlink(tmpfilename);
  unlink(outputfilename);
  
  //retval = 2*gene[0]*gene[1] + 2*gene[0] - gene[0]*gene[0] - 2*gene[1]*gene[1];
  //retval *= -1;
  //sleep(2);

  Ch_VaEnd(interp, ap);
  pthread_mutex_lock(&callback_lock);
  if(retval < lowest_gene) {
    lowest_gene = retval;
    fp = fopen("lowest_gene.txt", "w");
    for(i = 0; i < GENE_SIZE; i++) {
      fprintf(fp, "%d\n", (int)gene[i]);
    }
    fclose(fp);
  }
  callback_num_instances--;
  pthread_cond_signal(&callback_cond);
  pthread_mutex_unlock(&callback_lock);
  return retval;
}

int handleRequest(fipa_acl_message_t* acl)
{
  const char* content;
  std::string geneStr;
  char buf[128];
  char *tmp;
  char *saveptr;
  int i;
  MCAgent_t* agents;
  int num_agents;
  char* agent_list;
  char* agent_name;
  fipa_acl_message_t* reply;
  content = MC_AclGetContent(acl);
  if(content == NULL) {
    return -1;
  }
  printf("Get message with content: %s\n", content);
  MATCH_CMD(content, "REQUEST_GENE") {
    reply = MC_AclReply(acl);
    MC_AclSetPerformative(reply, FIPA_INFORM);
    /* Create the gene */
    geneStr = "GENE ";
    if(g_geneQueue.size() > 0) {
      geneStr += g_geneQueue.front()->str();
      g_geneQueue.pop();
    } else {
      std::ostringstream strm;
      for(i = 0; i < GENE_SIZE; i++) {
        double gene;
        gene = (double)rand()/(double)RAND_MAX * 256;
        strm << gene << ' ';
      }
      geneStr += strm.str();
    }
    MC_AclSetContent(reply, geneStr.c_str());
    sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
    MC_AclSetSender(reply, "master", buf);
    MC_AclSend(agency, reply);
    MC_AclDestroy(reply);
  } else 
  MATCH_CMD(content, "REQUEST_AGENTS") {
    printf("Master handling request for agents...\n");
    reply = MC_AclReply(acl);
    MC_AclSetPerformative(reply, FIPA_INFORM);
    sprintf(buf, "http://%s:%d/acc", g_hostname, g_localport);
    MC_AclSetSender(reply, "master", buf);
    MC_AgentProcessingBegin(agency);
    MC_GetAllAgents(agency, &agents, &num_agents);
    agent_list = (char*)malloc(20 * num_agents);
    *agent_list = '\0';
    sprintf(agent_list, "AGENTS %d ", num_agents - 1);
    for(i = 0; i < num_agents; i++) {
      agent_name = MC_GetAgentName(agents[i]);
      if(!strcmp(agent_name, "master")) {
        continue;
      }
      if(!strcmp(agent_name, "newscast")) {
        continue;
      }
      if(!strcmp(agent_name, "cullAgent")) {
        continue;
      }
      strcat(agent_list, agent_name);
      strcat(agent_list, " ");
      free(agent_name);
    }
    MC_AclSetContent(reply, agent_list);
    MC_AclSend(agency, reply);
    MC_AgentProcessingEnd(agency);
    free(agent_list);
    MC_AclDestroy(reply);
  } else
  MATCH_CMD(content, "REQUEST_CHILD") {
    std::istringstream iss(content+strlen("REQUEST_CHILD "));
    double value;
    int values[GENE_SIZE];
    for(i = 0; iss >> value; i++) {
      values[i] = value;
    }
    g_geneQueue.push(new Gene(values, GENE_SIZE));
    /* Add a new agent */
    /* If there are too many cost-functions running already, cancel starting the new agent. */
    pthread_mutex_lock(&callback_lock);
    if(callback_num_instances < CALLBACK_MAX_INSTANCES) {
      pthread_mutex_unlock(&callback_lock);
      startAgent(agency);
    } else {
      pthread_mutex_unlock(&callback_lock);
    }
  }
  MC_AclDestroy(acl);
  return 0;
}

int handleInform(fipa_acl_message_t* acl)
{
  /* TODO */
  return 0;
}

int handleDefault(fipa_acl_message_t* acl)
{
  /* TODO */
  return 0;
}
