#ifndef INVEST_BUDGET_H
#define INVEST_BUDGET_H

#include <stddef.h>

#define BUDGET_MAX_ITEMS 128
#define BUDGET_MAX_NAME 64
#define BUDGET_CATEGORY_COUNT 8

typedef enum { BUDGET_INCOME = 0, BUDGET_EXPENSE = 1 } BudgetKind;
typedef enum {
    CAT_MORADIA, CAT_ALIMENTACAO, CAT_TRANSPORTE, CAT_SAUDE,
    CAT_EDUCACAO, CAT_LAZER, CAT_CONTAS, CAT_OUTROS
} BudgetCategory;

typedef struct {
    char name[BUDGET_MAX_NAME];
    double amount;
    BudgetKind kind;
    int category;
    int fixed; /* 1: fixa; 0: variavel. Receita ignora este campo. */
} BudgetItem;

typedef struct {
    BudgetItem items[BUDGET_MAX_ITEMS];
    int count;
} BudgetStore;

typedef struct {
    double income;
    double expenses;
    double fixed_expenses;
    double variable_expenses;
    double by_category[BUDGET_CATEGORY_COUNT];
    double balance;
} BudgetTotals;

extern const char *const budget_categories[BUDGET_CATEGORY_COUNT];
int budget_valid_item(const BudgetItem *item);
int budget_totals(const BudgetStore *store, BudgetTotals *out);
int budget_path(char *out, size_t size);
int budget_load(BudgetStore *store, char *err, size_t err_size);
int budget_save(const BudgetStore *store, char *err, size_t err_size);

#endif
