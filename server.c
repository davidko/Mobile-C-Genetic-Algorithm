
#include <stdio.h>
#include <libmc.h>
#include <fipa_acl.h>
#include <string.h>

#define MATCH_CMD(str, cmd) \
  if (!strncmp(str, cmd, strlen(cmd)))

int agentCallbackFunc(ChInterp_t interp, MCAgent_t agent, void* user_data);
EXPORTCH double cost_chdl(void* varg);
void* masterAgentFunc(stationary_agent_info_t* agent_info);

int handleInform(fipa_acl_message_t* acl);
int handleDefault(fipa_acl_message_t* acl);
int handleRequest(fipa_acl_message_t* acl);

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
  
  agency = MC_Initialize(local_port, &options);
  MC_AddAgentInitCallback(agency, agentCallbackFunc, NULL);

  /* Start the master handler agent */
  MC_AddStationaryAgent(agency, masterAgentFunc, "master", NULL);

  while(1) {
  /* Every five seconds or so, get a list of all the agents and kill enough
   * agents so that we have a standing population of 30 agents. */
    sleep(5);
  }

  MC_End(agency);
  return 0;
}

void* masterAgentFunc(stationary_agent_info_t* agent_info)
{
  int i;
  fipa_acl_message_t* acl;
  /* Start some agents */
  for(i = 0; i < 5; i++) {
    startAgent(agent_info->agency);
  }
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
  
  MC_AddAgent(agency, agent);
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
  printf("Server got request: %s\n", content);
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
    printf("Server got request agents...\n");
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
