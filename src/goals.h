#ifndef INVEST_GOALS_H
#define INVEST_GOALS_H
#include <stddef.h>

#define GOALS_MAX 64
#define GOALS_ALLOC_MAX 2048
#define GOAL_NAME_MAX 64

typedef struct {
    int id;
    char name[GOAL_NAME_MAX];
    long long target_cents;
    int deadline_year, deadline_month;
} Goal;

typedef struct {
    int goal_id, year, month;
    long long cents;
} GoalAllocation;

typedef struct {
    Goal items[GOALS_MAX];
    int count;
    GoalAllocation allocations[GOALS_ALLOC_MAX];
    int allocation_count;
    int next_id;
} GoalsStore;

int goals_path(char *out, size_t cap);
/* Return 0 for loaded, 1 for missing (empty), -1 for corrupt/unreadable. */
int goals_load(GoalsStore *store, char *err, size_t cap);
int goals_load_file(const char *path, GoalsStore *store, char *err, size_t cap);
int goals_validate_file(const char *path, void *context);
int goals_save(const GoalsStore *store, char *err, size_t cap);
int goals_valid(const GoalsStore *store);
int goals_add(GoalsStore *store, const char *name, long long target_cents,
              int deadline_year, int deadline_month);
int goals_edit(GoalsStore *store, int id, const char *name, long long target_cents,
               int deadline_year, int deadline_month);
/* Deletion only after allocations have been manually unassigned. */
int goals_delete(GoalsStore *store, int id);
int goals_allocate(GoalsStore *store, int goal_id, int year, int month,
                   long long amount_cents, long long recorded_cents);
long long goals_month_total(const GoalsStore *store, int year, int month);
long long goals_month_for(const GoalsStore *store, int goal_id, int year, int month);
long long goals_progress(const GoalsStore *store, int goal_id, int through_year, int through_month);
int goals_find(const GoalsStore *store, int id);
/* Centavos por mês necessários após o mês selecionado, sem juros.
 * -1 quando não há prazo futuro e a meta ainda não foi alcançada. */
long long goals_required_monthly(const GoalsStore *store, int id, int year, int month);
#endif
