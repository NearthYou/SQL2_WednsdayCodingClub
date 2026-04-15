#ifndef MINISQL_PRINTER_H
#define MINISQL_PRINTER_H

#include "bptree.h"
#include "db.h"
#include "error.h"

#include <stdio.h>

void printer_print_rows(FILE *out, const Table *table, const RowIndexList *rows);
void printer_print_error(FILE *out, const SqlError *error);
void printer_print_transaction_rolled_back(FILE *out);
void printer_print_transaction_committed(FILE *out);

#endif
