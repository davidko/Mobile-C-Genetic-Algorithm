
#include <stdio.h>
#include <libmc.h>
int agentCallbackFunc(ChInterp_t interp, MCAgent_t agent, void* user_data);
EXPORTCH double cost_chdl(void* varg);

double points[3][2] = {2.4, 8.7, 2.1, 9.8, 6.5, 8.8};

int main() 
{
  MCAgency_t agency;
  int local_port = 5051;
  setbuf(stdout, NULL);
  
  agency = MC_Initialize(local_port, NULL);
  MC_AddAgentInitCallback(agency, agentCallbackFunc, NULL);

  MC_MainLoop(agency);

  MC_End(agency);
  return 0;
}

/* This callback function is called during the initialization step of each
 * incoming agent. We will add a c-space function to each interpreter that
 * the agent will be able to call. */
int agentCallbackFunc(ChInterp_t interp, MCAgent_t agent, void* user_data)
{
  Ch_DeclareFunc(interp, "double costFunction(double x[3]);", (ChFuncdl_t)cost_chdl);
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
  int i = 0;
  retval = 0;
  double value;
  for(i = 0; i < 3; i++) {
    value = x[2]*points[i][0]*points[i][0] + x[1]*points[i][0] + x[0];
    value = value - points[i][1];
    value = value * value;
    retval += value;
  }

  Ch_VaEnd(interp, ap);
  return retval;
}
