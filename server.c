
#include <stdio.h>
#include <libmc.h>
#include <fipa_acl.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define MATCH_CMD(str, cmd) \
  if (!strncmp(str, cmd, strlen(cmd)))

#define AGENT_POPULATION 75

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

/* When an agent requests a reproduction, it will send a message to the server
 * saying it wants to create a child with a certain gene. The server will then
 * store that gene in the following variable and create a generic child agent.
 * When the child agent initializes, it will ask the server for a gene, at
 * which time the server will provide the following gene, and unset the flag.
 * If an agent asks for a gene and the flag is not set, the server will create
 * a random gene. */
double gene[20];
int gene_flag;

double points[20][2];

MCAgency_t agency;

stationary_agent_info_t* g_master_agent;

struct agent_info_s {
  char name[40];
  double fitness;
};

int main() 
{
  MCAgencyOptions_t options;
  MC_InitializeAgencyOptions(&options);
  MC_SetThreadOff(&options, MC_THREAD_CP);
  int local_port = 5051;
  int i;
  gene_flag = 0;
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
  
  agency = MC_Initialize(local_port, &options);
  MC_AddAgentInitCallback(agency, agentCallbackFunc, NULL);

  /* Add qsort mutex */
  MC_SyncInit(agency, 875);

  /* Start the master handler agent */
  MC_AddStationaryAgent(agency, masterAgentFunc, "master", NULL);
  /* Start the agent responsible for controlling the population */
  MC_AddStationaryAgent(agency, cullAgentFunc, "cullAgent", NULL);

  /* Start some agents */
  for(i = 0; i < 20; i++) {
    startAgent(agency);
    usleep(500000);
  }

  AgentInfo_t *agentList;
  FILE *logfile;
  logfile = fopen("logfile.txt", "w");
  while(1) {
  /* Every five seconds or so, get a list of all the agents and kill enough
   * agents so that we have a standing population of 30 agents. */
    sleep(10);
    composeSortedAgentList(agency, &agentList, &num_agents);
    /* Calculate avg fitness */
    int i;
    double avgfitness = 0;
    for(i = 0; i < num_agents; i++) {
      avgfitness += agentList[i].fitness;
    }
    avgfitness /= (double)num_agents;
    fprintf(logfile, "%d %lf\n", num_agents, avgfitness);
    fflush(logfile);
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
  while(1) {
    sleep(2);
    composeSortedAgentList(agency, &agentList, &numAgents);
    if(numAgents > AGENT_POPULATION) {
      /* Terminate the end of the list */
      for(i = AGENT_POPULATION; i < numAgents; i++) {
        /* Send termination message to each of these agents */
        msg = MC_AclNew();
        MC_AclSetPerformative(msg, FIPA_INFORM);
        MC_AclAddReceiver(msg, (agentList[i]).name, "http://localhost:5051/acc");
        MC_AclSetContent(msg, "TERMINATE");
        MC_AclSetSender(msg, "master", "http://localhost:5051/acc");
        MC_AclSetConversationID(msg, "none");
        MC_AclSend(agency, msg);
        MC_AclDestroy(msg);
      }
    }
    free(agentList);
  }
}

int composeSortedAgentList(MCAgency_t agency, AgentInfo_t **agentList, int *numAgents)
{
  MCAgent_t* agents;
  int num, i, j;
  char* name;
  double* fitness;
  size_t size;
  MC_GetAllAgents(agency, &agents, &num);
  *agentList = (AgentInfo_t*)malloc(sizeof(AgentInfo_t)*num);
  j = 0;
  for(i = 0; i < num; i++) {
    name = MC_GetAgentName(agents[i]);
    if(!strcmp(name, "master") || !strcmp(name, "cullAgent")) {
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
  *numAgents = j-1;

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
  double gene[20];
  /* Generate a random name */
  char name[80];
  char content[400];
  char buf[80];
  int i;
  sprintf(name, "agent%d", rand());
  agent = MC_ComposeAgentFromFile(
      name,
      "localhost:5051",
      "IEL", 
      "agent.c",
      NULL,
      "localhost:5051",
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
  return 0; /* 0 for success error status */
}

EXPORTCH double cost_chdl(void* varg)
{
  pthread_mutex_lock(&callback_lock);
  double retval;
  double *x;
  ChInterp_t interp;
  ChVaList_t ap;

  Ch_VaStart(interp, ap, varg);
  x = Ch_VaArg(interp, ap, double*);
  int i, j;
  retval = 0;
  double value = 0, term;
  for(i = 0; i < 20; i++) {
    term = 1;
    for(j = 0; j < i; j++) {
      term *= points[i][0];
    }
    term *= x[i];
    value = term - points[i][1];
    retval += value * value;
  }

  Ch_VaEnd(interp, ap);
  pthread_mutex_unlock(&callback_lock);
  return retval;
}

int handleRequest(fipa_acl_message_t* acl)
{
  const char* content;
  char geneStr[400];
  char buf[80];
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
  MATCH_CMD(content, "REQUEST_GENE") {
    reply = MC_AclReply(acl);
    MC_AclSetPerformative(reply, FIPA_INFORM);
    /* Create the gene */
    sprintf(geneStr, "GENE ");
    for(i = 0; i < 20; i++) {
      if(!gene_flag) {
        gene[i] = (double) (rand() % 100) - 50;
      }
      sprintf(buf, "%lf", gene[i]);
      strcat(geneStr, buf);
      strcat(geneStr, " ");
    }
    gene_flag = 0;
    MC_AclSetContent(reply, geneStr);
    MC_AclSetSender(reply, "master", "http://localhost:5051/acc");
    MC_AclSend(agency, reply);
    MC_AclDestroy(reply);
  } else 
  MATCH_CMD(content, "REQUEST_AGENTS") {
    printf("Master handling request for agents...\n");
    reply = MC_AclReply(acl);
    MC_AclSetPerformative(reply, FIPA_INFORM);
    MC_AclSetSender(reply, "master", "http://localhost:5051/acc");
    MC_GetAllAgents(agency, &agents, &num_agents);
    agent_list = (char*)malloc(20 * num_agents);
    *agent_list = '\0';
    sprintf(agent_list, "AGENTS %d ", num_agents - 1);
    for(i = 0; i < num_agents; i++) {
      agent_name = MC_GetAgentName(agents[i]);
      if(!strcmp(agent_name, "master")) {
        continue;
      }
      strcat(agent_list, agent_name);
      strcat(agent_list, " ");
      free(agent_name);
    }
    MC_AclSetContent(reply, agent_list);
    MC_AclSend(agency, reply);
    free(agent_list);
    MC_AclDestroy(reply);
  } else
  MATCH_CMD(content, "REQUEST_CHILD") {
    strcpy(geneStr, content + strlen("REQUEST_CHILD"));
    tmp = strtok_r(geneStr, " \t", &saveptr);
    i = 0;
    while(tmp != NULL && i < 20) {
      sscanf(tmp, "%lf", &gene[i]);
      i++;
    }
    gene_flag = 1;
    /* Add a new agent */
    startAgent(agency);
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
