
#include <stdio.h>
#include <libmc.h>
#include <fipa_acl.h>
#include <string.h>

#define MATCH_CMD(str, cmd) \
  if (!strncmp(str, cmd, strlen(cmd)))

int agentCallbackFunc(ChInterp_t interp, MCAgent_t agent, void* user_data);
EXPORTCH double cost_chdl(void* varg);
void* masterAgentFunc(stationary_agent_info_t* agent_info);

double points[20][2];

int main() 
{
  MCAgency_t agency;
  MCAgencyOptions_t options;
  MC_InitializeAgencyOptions(&options);
  MC_SetThreadOff(&options, MC_THREAD_CP);
  int local_port = 5051;
  int i;
  double *point = &(points[0][0]);
  MCAgent_t agent;
  srand(time(NULL));
  for(i = 0; i < 40; i++) {
    *point = (rand() % 100) - 50;
    point++;
  }
  
  agency = MC_Initialize(local_port, &options);
  MC_AddAgentInitCallback(agency, agentCallbackFunc, NULL);

  /* Start the master handler agent */
  MC_AddStationaryAgent(agency, masterAgentFunc, "master", NULL);

  MC_MainLoop(agency);

  MC_End(agency);
  return 0;
}

void* masterAgentFunc(stationary_agent_info_t* agent_info)
{
  fipa_acl_message_t* acl;
  /* Start an agent */
  while(1) {
    acl = MC_AclWaitRetrieve(agent_info->agency);
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
  
  /* Send the agent a gene */
  acl = MC_AclNew();
  MC_AclSetSender(acl, "master", "http://localhost:5051/acc");
  MC_AclSetPerformative(acl, FIPA_INFORM);
  MC_AclAddReceiver(acl, name, NULL);
  /* Create the gene */
  sprintf(content, "SET_GENE ");
  for(i = 0; i < 20; i++) {
    gene[i] = (double) (rand() % 100) - 50;
    sprintf(buf, "%lf", gene[i]);
    strcat(content, buf);
    strcat(content, " ");
  }
  MC_AclSetContent(acl, content);
  MC_AclPost(agent, acl);
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
