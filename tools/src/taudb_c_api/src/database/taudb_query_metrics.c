#include "taudb_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

TAUDB_METRIC* taudb_query_metrics(TAUDB_CONNECTION* connection, TAUDB_TRIAL* trial) {
#ifdef TAUDB_DEBUG_DEBUG
  printf("Calling taudb_query_metrics(%p)\n", trial);
#endif
  int nFields;
  int i, j;

  if (trial == NULL) {
    fprintf(stderr, "Error: trial parameter null. Please provide a valid trial.\n");
    return NULL;
  }

  //if the Trial already has the data, return it.
  if (trial->metrics_by_id != NULL) {
    taudb_numItems = HASH_CNT(hh1,trial->metrics_by_id);
    return trial->metrics_by_id;
  }

  taudb_begin_transaction(connection);

  /*
   * Fetch rows from table_name, the system catalog of databases
   */
  char my_query[256];
  sprintf(my_query,"select * from metric where trial = %d", trial->id);
#ifdef TAUDB_DEBUG
  printf("Query: %s\n", my_query);
#endif
  taudb_execute_query(connection, my_query);

  int nRows = taudb_get_num_rows(connection);
  taudb_numItems = nRows;

  //TAUDB_METRIC* metrics = taudb_create_metrics(taudb_numItems);
  // allocation handled by the UThash structure!
  trial->metrics_by_id = NULL;
  trial->metrics_by_name = NULL;

  nFields = taudb_get_num_columns(connection);

  /* the rows */
  for (i = 0; i < taudb_get_num_rows(connection); i++)
  {
    TAUDB_METRIC* metric = taudb_create_metrics(1);
    /* the columns */
    for (j = 0; j < nFields; j++) {
	  if (strcmp(taudb_get_column_name(connection, j), "id") == 0) {
	    metric->id = atoi(taudb_get_value(connection, i, j));
	  } else if (strcmp(taudb_get_column_name(connection, j), "trial") == 0) {
	    //metric->trial = trial;
	  } else if (strcmp(taudb_get_column_name(connection, j), "name") == 0) {
	    metric->name = taudb_strdup(taudb_get_value(connection,i,j));
	  } else if (strcmp(taudb_get_column_name(connection, j), "derived") == 0) {
	    metric->derived = atoi(taudb_get_value(connection, i, j));
	  } else {
	    printf("Error: unknown column '%s'\n", taudb_get_column_name(connection, j));
	    taudb_exit_nicely(connection);
	  }
	} 
	HASH_ADD(hh1, trial->metrics_by_id, id, sizeof(int), metric);
	HASH_ADD_KEYPTR(hh2, trial->metrics_by_name, metric->name, strlen(metric->name), metric);
  }

  taudb_clear_result(connection);
  taudb_close_transaction(connection);

  return (trial->metrics_by_id);
}

TAUDB_METRIC* taudb_get_metric_by_name(TAUDB_METRIC* metrics, const char* name) {
#ifdef TAUDB_DEBUG_DEBUG
  printf("Calling taudb_get_metric_by_id(%p,%s)\n", metrics, name);
#endif
  if (metrics == NULL) {
    fprintf(stderr, "Error: metric parameter null. Please provide a valid set of metrics.\n");
    return NULL;
  }
  if (name == NULL) {
    fprintf(stderr, "Error: name parameter null. Please provide a valid name.\n");
    return NULL;
  }

  TAUDB_METRIC* metric = NULL;
  HASH_FIND(hh2, metrics, name, strlen(name), metric);
  return metric;
}

TAUDB_METRIC* taudb_get_metric_by_id(TAUDB_METRIC* metrics, const int id) {
#ifdef TAUDB_DEBUG_DEBUG
  printf("Calling taudb_get_metric_by_id(%p,%d)\n", metrics, id);
#endif
  if (metrics == NULL) {
    fprintf(stderr, "Error: metric parameter null. Please provide a valid set of metrics.\n");
    return NULL;
  }
  if (id == 0) {
    fprintf(stderr, "Error: name parameter null. Please provide a valid name.\n");
    return NULL;
  }

  TAUDB_METRIC* metric = NULL;
  HASH_FIND(hh1, metrics, &id, sizeof(int), metric);
  return metric;
}

void taudb_save_metrics(TAUDB_CONNECTION* connection, TAUDB_TRIAL* trial, boolean update) {
  const char* my_query = "insert into metric (trial, name, derived) values ($1, $2, $3);";
  const char* statement_name = "TAUDB_INSERT_METRIC";
  taudb_prepare_statement(connection, statement_name, my_query, 3);
  TAUDB_METRIC *metric, *tmp;
  HASH_ITER(hh2, trial->metrics_by_name, metric, tmp) {
    // make array of 6 character pointers
    const char* paramValues[3] = {0};
    char trialid[32] = {0};
    sprintf(trialid, "%d", trial->id);
    paramValues[0] = trialid;
    paramValues[1] = metric->name;
    char derived[32] = {0};
    sprintf(derived, "%d", metric->derived);
    paramValues[2] = derived;

    taudb_execute_statement(connection, statement_name, 3, paramValues);
    taudb_execute_query(connection, "select currval('metric_id_seq');");

    int nRows = taudb_get_num_rows(connection);
    if (nRows == 1) {
      metric->id = atoi(taudb_get_value(connection, 0, 0));
      printf("New Metric: %d\n", metric->id);
    } else {
      printf("Failed.\n");
    }
	taudb_close_query(connection);
  }
  taudb_clear_result(connection);
}

