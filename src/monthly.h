#ifndef INVEST_MONTHLY_H
#define INVEST_MONTHLY_H
#include "budget.h"
#include <stddef.h>

/* One calendar month, with independent planned and recorded values. */
typedef struct {
    BudgetStore planned;
    BudgetStore actual;
} MonthlyBudget;

/* Return 0 for success; 1 if no file exists; -1 for invalid/unreadable data. */
int monthly_load(int year, int month, MonthlyBudget *month_data, char *err, size_t err_size);
int monthly_load_file(const char *path, int year, int month, MonthlyBudget *month_data, char *err, size_t err_size);
/* context = int date[2] containing year/month */
int monthly_validate_file(const char *path, void *context);
int monthly_save(int year, int month, const MonthlyBudget *month_data, char *err, size_t err_size);
int monthly_path(int year, int month, char *out, size_t cap);
#endif
