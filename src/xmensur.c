/*
 * XMENSUR file format parser
 * Rewritten to follow zmensur.c structure with 4 phases:
 * 1. Read and parse variables
 * 2. Read and store group definitions (child mensurs)
 * 3. Build main mensur structure
 * 4. Resolve connections and rejoint branches
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include "zmensur.h"
#include "kutils.h"
#include "xmensur.h"
#include "tinyexpr.h"

#define MAX_VARS 256
#define MAX_GROUPS 256

/* Variable storage */
typedef struct {
    char name[64];
    double value;
} xmen_var;

/* Group (child mensur) storage */
typedef struct {
    char name[256];
    mensur *men;
} xmen_group;

static xmen_var variables[MAX_VARS];
static int var_count = 0;

static xmen_group groups[MAX_GROUPS];
static int group_count = 0;

/* Forward declarations */

/*
 * Evaluate arithmetic expression with variables using TinyExpr
 * Supports: +, -, *, /, (), sin, cos, tan, sqrt, pi, etc.
 */
static double evaluate_expression(char *expr) {
    /* Build array of te_variable for TinyExpr */
    te_variable *te_vars = malloc(var_count * sizeof(te_variable));
    for (int i = 0; i < var_count; i++) {
        te_vars[i].name = variables[i].name;
        te_vars[i].address = &variables[i].value;
        te_vars[i].type = TE_VARIABLE;
        te_vars[i].context = NULL;
    }

    /* Compile and evaluate expression */
    int err;
    te_expr *compiled = te_compile(expr, te_vars, var_count, &err);
    double result = 0.0;

    if (compiled) {
        result = te_eval(compiled);
        te_free(compiled);
    } else {
        fprintf(stderr, "Error parsing expression '%s' at position %d\n", expr, err);
    }

    free(te_vars);
    return result;
}





/*
 * Phase 3: Build main mensur structure from MAIN section
 */
mensur* build_xmen(mensur* men) {
    mensur *cur = NULL;

    /* write code here to buildup xmensur */
    return men;
}

/* Find xmen which has specified name */
mensur* find_xmen(char* name){
    mensur* men = NULL;

    for (int i = 0; i < group_count; i++) {
        if (strcmp(groups[i].name, name) == 0) {
            men = groups[i].men;
            break;
        }
    }
    return men;
}

/*
 * Phase 4: Resolve child mensur connections
 */
static void resolve_xmen_child(mensur *men) {
    mensur *m = men;  /* Use men directly, don't call get_first_men */

    while (m != NULL) {
        if (m->sidename[0] != '\0') {
            mensur* child = find_xmen(m->sidename);
            if (child != NULL){
                if( m->s_type != JOIN) {
                    /* SPLIT or BRANCH */
                    m->side = child;
                    resolve_xmen_child(child); /* recursive call for resolving */
                }else{
                    /* JOIN */
                    m->side = get_last_men(child); /* join last element of child branch */
                }
            }else{
                fprintf("cannot find corresponding child : \"%s\"", m->sidename);
            }
        }
        m = m->next;
    }
}

/*
 * Rejoint mensur based on side connection ratio
 * Same logic as rejoint_men in zmensur.c
 */
static mensur* rejoint_xmen(mensur* men) {
    mensur *p, *q, *s, *ss;

    p = men;  /* Use men directly, already pointing to first segment */

    while (p->next != NULL) {
        if (p->s_ratio > 0.5) {
            if (p->side != NULL && p->s_type == ADDON) {
                /* 繋ぎ直し for ADDON */
                q = get_last_men(p->side);
                q = remove_men(q); /* 最後は除く(1個前が返される) */
                s = p->next;

                q->next = s;
                s->prev = q;
                p->next = p->side;
                p->side->prev = p;
                ss = create_men(s->df, s->db, s->r, s->comment);
                p->side = ss;
                p->s_ratio = 1 - p->s_ratio;
                append_men(ss, ss->db, 0, 0, "");
            } else if (p->side != NULL && p->s_type == SPLIT) {
                /* join先を探す for SPLIT */
                s = get_join_men(p, p->side);

                q = s->side;
                q = remove_men(q); /* 最後は除く(1個前が返される) */

                ss = p->next;
                p->side->prev = p;
                p->next = p->side;
                p->side = ss;
                ss->prev = NULL;
                p->s_ratio = 1 - p->s_ratio;

                s->next->prev = q;
                q->next = s->next;
                s->next = s->side = NULL;
                s->s_type = 0;

                ss = append_men(s, s->db, 0, 0, "");
                q->side = ss;
                q->s_type = JOIN;
                q->s_ratio = 1 - s->s_ratio;
                s->s_ratio = 0;
            }

            /* recursive call to rejoint orginal branch */
            /* rejoint_xmen(p); */
        } else {
            /* s_ratio < 0.5なのでメインのメンズールには変更がない */
        }

        p = p->next;
    }

    return men; /* same value that used input */
}


/* 
    main file reading function for xmensur 
    xmensur file type is defined in doc/xmensur.md
*/
mensur* read_xmensur(const char* path){
    mensur* head = NULL;

    /* first, read all contents of xmensur file ard return it as a list of linetext. 
    ignore blank lines and comment. remove whitespaces
    */
    char** lines = read_xmensur_text(path);

    /* read variable definition lines from other marker and cell lines */
    char** vardefs = split_var_defs(lines);
    xmen_var* vars = read_variables(vardefs);

    /* validate marker lines and create mensur list */
    char** mendefs = split_men_defs(lines);

    /* call `read_group` in this function. 
    make sure GROUP can be nested. 
    so that read_group would be called recursively if GROUP marker is found 
    */
    xmen_group* mens = read_xmen_groups(mendefs);

    /* pointer to head mensur of MAIN */
    mensur* mainmen = get_main_xmen(mens);

    /* resolve INSERT,SPLIT, BRANCH, MERGE of groups. while realizing variables. */
    mainmen = build_xmen(mainmen);

    /* in case of BRANCH/MERGE, replace main trunc if s_ratio > 0.5 */
    mainmen = rejoint_xmen(mainmen);

    return mainmen;
}

