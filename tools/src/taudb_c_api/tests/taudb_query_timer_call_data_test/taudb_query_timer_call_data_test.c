#include "taudb_api.h"
#include <stdio.h>
#include <string.h>
#include "dump_functions.h"

int main (int argc, char** argv) {
   printf("Connecting...\n");
   TAUDB_CONNECTION* connection = NULL;
   if (argc >= 2) {
     connection = taudb_connect_config(argv[1]);
   } else {
     fprintf(stderr, "Please specify a TAUdb config file.\n");
     exit(1);
   }
   printf("Checking connection...\n");
   taudb_check_connection(connection);
   printf("Testing queries...\n");

   int t;

   TAUDB_TRIAL* filter = taudb_create_trials(1);
   filter->id = atoi(argv[2]);
   TAUDB_TRIAL* trials = taudb_query_trials(connection, FALSE, filter);
   int numTrials = taudb_numItems;
   for (t = 0 ; t < numTrials ; t = t+1) {
      printf("  Trial name: '%s', id: %d\n", trials[t].name, trials[t].id);
      dump_timer_call_data(connection, &(trials[t]), TRUE);
   }

   printf("Disconnecting...\n");
   taudb_disconnect(connection);
   printf("Done.\n");
return 0;
}
