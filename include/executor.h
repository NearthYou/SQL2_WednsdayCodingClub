#ifndef MINISQL_EXECUTOR_H
#define MINISQL_EXECUTOR_H

#include "db.h"
#include "error.h"
#include "parser.h"

#include <stdbool.h>
#include <stdio.h>

bool execute_batch(Database *db, const QueryBatch *batch, FILE *out, SqlError *error);

#endif
