/*
 * ss_err.c:
 * This file is automatically generated; please do not edit it.
 */

#include <stdlib.h>

static const char * const text[] = {
		"Subsystem aborted",
		"Version mismatch",
		"No current invocation",
		"No info directory",
		"Command not found",
		"Command line aborted",
		"End-of-file reached",
		"Permission denied",
		"Request table not found",
		"No info available",
		"Shell escapes are disabled",
		"Sorry, this request is not yet implemented",
    0
};

struct error_table {
    char const * const * msgs;
    long base;
    int n_msgs;
};
struct et_list {
    struct et_list *next;
    const struct error_table * table;
};
struct et_list *_et_list;

const struct error_table et_ss_error_table = { text, 748800L, 12 };

static struct et_list link = { 0, 0 };

void initialize_ss_error_table_r(struct et_list **list);
void initialize_ss_error_table(void);

void initialize_ss_error_table(void) {
    initialize_ss_error_table_r(&_et_list);
}

/* For Heimdal compatibility */
void initialize_ss_error_table_r(struct et_list **list)
{
    struct et_list *et, **end;

    for (end = list, et = *list; et; end = &et->next, et = et->next)
        if (et->table->msgs == text)
            return;
    et = malloc(sizeof(struct et_list));
    if (et == 0) {
        if (!link.table)
            et = &link;
        else
            return;
    }
    et->table = &et_ss_error_table;
    et->next = 0;
    *end = et;
}
