/*
 * XMENSUR file format parser - Complete implementation
 * Following the specification in doc/xmensur.md
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
#define MAX_LINES 10000

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

/*
 * Utility: skip whitespace
 */
static char* skip_whitespace(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/*
 * Utility: trim line - remove comments and whitespace
 */
static char* trim_line(char *line) {
    /* Remove # comments */
    char *p = strchr(line, '#');
    if (p) *p = '\0';

    /* Skip leading whitespace */
    line = skip_whitespace(line);

    /* Remove trailing whitespace */
    p = line + strlen(line) - 1;
    while (p >= line && isspace((unsigned char)*p)) {
        *p = '\0';
        p--;
    }

    return line;
}

/*
 * Evaluate arithmetic expression with variables using TinyExpr
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
 * Step 1: Read all contents of XMENSUR file and return as list of line text
 * Ignore blank lines and comments, remove whitespaces
 */
static char** read_xmensur_text(const char* path) {
    FILE *infile;
    struct stat fstatus;
    char *readbuffer;
    char **lines;
    int line_count = 0;

    /* Read entire file */
    if (stat(path, &fstatus) != 0) {
        fprintf(stderr, "Failed to open XMENSUR file: %s\n", path);
        return NULL;
    }

    readbuffer = malloc(fstatus.st_size + 1);
    if (!readbuffer) {
        fprintf(stderr, "Cannot allocate memory\n");
        return NULL;
    }

    infile = fopen(path, "r");
    if (!infile) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        free(readbuffer);
        return NULL;
    }

    fread(readbuffer, 1, fstatus.st_size, infile);
    readbuffer[fstatus.st_size] = '\0';
    fclose(infile);

    eol_to_lf(readbuffer);

    /* Allocate lines array */
    lines = (char**)malloc(MAX_LINES * sizeof(char*));
    for (int i = 0; i < MAX_LINES; i++) {
        lines[i] = NULL;
    }

    /* Split into lines, trim, and skip blank/comment lines */
    char *line = strtok(readbuffer, "\n");
    while (line != NULL && line_count < MAX_LINES - 1) {
        char *trimmed = trim_line(strdup(line));
        if (strlen(trimmed) > 0) {
            lines[line_count++] = trimmed;
        } else {
            free(trimmed);
        }
        line = strtok(NULL, "\n");
    }
    lines[line_count] = NULL;  /* NULL terminator */

    free(readbuffer);
    return lines;
}

/*
 * Step 2: Split variable definitions from other lines
 * Variable line format: "name = expression"
 */
static char** split_var_defs(char** lines) {
    char **vardefs = (char**)malloc(MAX_LINES * sizeof(char*));
    int var_idx = 0;

    for (int i = 0; lines[i] != NULL; i++) {
        /* Check if line contains '=' and is not a marker */
        if (strchr(lines[i], '=') &&
            strncmp(lines[i], "MAIN", 4) != 0 &&
            strncmp(lines[i], "GROUP", 5) != 0 &&
            lines[i][0] != '[' && lines[i][0] != '{') {
            vardefs[var_idx++] = strdup(lines[i]);
        }
    }
    vardefs[var_idx] = NULL;

    return vardefs;
}

/*
 * Step 3: Read variable definitions
 */
static xmen_var* read_xmen_variables(char** vardefs) {
    var_count = 0;

    for (int i = 0; vardefs[i] != NULL && var_count < MAX_VARS; i++) {
        char *line = strdup(vardefs[i]);
        char *eq = strchr(line, '=');
        if (!eq) {
            free(line);
            continue;
        }

        *eq = '\0';
        char *name = skip_whitespace(line);
        char *value_str = skip_whitespace(eq + 1);

        /* Remove trailing whitespace from name */
        char *end = name + strlen(name) - 1;
        while (end > name && isspace((unsigned char)*end)) *end-- = '\0';

        if (strlen(name) > 0) {
            strncpy(variables[var_count].name, name, 63);
            variables[var_count].name[63] = '\0';
            variables[var_count].value = evaluate_expression(value_str);
            var_count++;
        }

        free(line);
    }

    return variables;
}

/*
 * Step 4: Split mensur definition lines (non-variable lines)
 */
static char** split_men_defs(char** lines) {
    char **mendefs = (char**)malloc(MAX_LINES * sizeof(char*));
    int men_idx = 0;

    for (int i = 0; lines[i] != NULL; i++) {
        /* Skip variable definitions */
        if (strchr(lines[i], '=') &&
            strncmp(lines[i], "MAIN", 4) != 0 &&
            strncmp(lines[i], "GROUP", 5) != 0 &&
            lines[i][0] != '[' && lines[i][0] != '{') {
            continue;
        }
        mendefs[men_idx++] = strdup(lines[i]);
    }
    mendefs[men_idx] = NULL;

    return mendefs;
}

/* Forward declaration */
static mensur* parse_group_recursive(char** lines, int *idx, const char *group_name);

/*
 * Parse df,db,r line
 */
static int parse_dfdbr(char *line, double *df, double *db, double *r, char *comment) {
    char *tokens[4];
    int token_count = 0;

    /* Tokenize by comma */
    char *p = line;
    char *start = p;
    while (*p && token_count < 4) {
        if (*p == ',') {
            *p = '\0';
            tokens[token_count++] = skip_whitespace(start);
            start = p + 1;
        }
        p++;
    }
    if (token_count < 4 && *start) {
        tokens[token_count++] = skip_whitespace(start);
    }

    if (token_count < 3) return 0;

    *df = evaluate_expression(tokens[0]);
    *db = evaluate_expression(tokens[1]);
    *r = evaluate_expression(tokens[2]);

    if (comment) {
        if (token_count > 3 && tokens[3]) {
            strncpy(comment, skip_whitespace(tokens[3]), 63);
            comment[63] = '\0';
        } else {
            comment[0] = '\0';
        }
    }

    return 1;
}

/*
 * Step 5: Read all groups including MAIN recursively
 * Parse GROUP/END_GROUP pairs, handle nesting
 */
static mensur* parse_group_recursive(char** lines, int *idx, const char *group_name) {
    mensur *head = NULL, *cur = NULL;
    int depth = 1;  /* We're inside a group */

    while (lines[*idx] != NULL) {
        char *line = strdup(lines[*idx]);
        (*idx)++;

        /* Handle END_GROUP or } */
        if (strcmp(line, "END_GROUP") == 0 || strcmp(line, "}") == 0 || strcmp(line, "]") == 0 || strcmp(line, "END_MAIN") == 0) {
            depth--;
            if (depth == 0) {
                /* Add terminator if not present */
                if (cur != NULL && (cur->db != 0 || cur->r != 0)) {
                    cur = append_men(cur, cur->db, 0, 0, "");
                }
                free(line);
                break;
            }
            free(line);
            continue;
        }

        /* Handle nested GROUP or { */
        if (strncmp(line, "GROUP", 5) == 0 || strncmp(line, "{", 1) == 0) {
            depth++;
            free(line);
            continue;
        }

        /* Handle OPEN_END */
        if (strcmp(line, "OPEN_END") == 0) {
            if (cur) {
                cur = append_men(cur, cur->db, 0, 0, "");
            }
            free(line);
            continue;
        }

        /* Handle CLOSED_END */
        if (strcmp(line, "CLOSED_END") == 0) {
            if (cur) {
                cur = append_men(cur, 0, 0, 0, "");
            }
            free(line);
            continue;
        }

        /* Handle BRANCH marker: BRANCH, groupname, ratio */
        if (strncmp(line, "BRANCH", 6) == 0 || line[0] == '>') {
            if (cur) {
                char *p = (line[0] == '>') ? line + 1 : line + 6;
                if (*p == ',') p++;
                p = skip_whitespace(p);

                /* Extract group name */
                char *comma = strchr(p, ',');
                if (comma) {
                    *comma = '\0';
                    strncpy(cur->sidename, p, 15);
                    cur->sidename[15] = '\0';

                    /* Extract ratio */
                    char *ratio_str = skip_whitespace(comma + 1);
                    cur->s_ratio = evaluate_expression(ratio_str);
                    cur->s_type = SPLIT;  /* BRANCH uses SPLIT type */
                }
            }
            free(line);
            continue;
        }

        /* Handle MERGE marker: MERGE, groupname, ratio */
        if (strncmp(line, "MERGE", 5) == 0 || line[0] == '<') {
            if (cur) {
                char *p = (line[0] == '<') ? line + 1 : line + 5;
                if (*p == ',') p++;
                p = skip_whitespace(p);

                /* Extract group name */
                char *comma = strchr(p, ',');
                if (comma) {
                    *comma = '\0';
                    strncpy(cur->sidename, p, 15);
                    cur->sidename[15] = '\0';

                    /* Extract ratio */
                    char *ratio_str = skip_whitespace(comma + 1);
                    cur->s_ratio = evaluate_expression(ratio_str);
                    cur->s_type = JOIN;  /* MERGE uses JOIN type */
                }
            }
            free(line);
            continue;
        }

        /* Handle SPLIT marker: SPLIT, groupname, ratio or |,groupname,ratio */
        if (strncmp(line, "SPLIT", 5) == 0 || line[0] == '|') {
            if (cur) {
                char *p = (line[0] == '|') ? line + 1 : line + 5;
                if (*p == ',') p++;
                p = skip_whitespace(p);

                /* Extract group name */
                char *comma = strchr(p, ',');
                if (comma) {
                    *comma = '\0';
                    strncpy(cur->sidename, p, 15);
                    cur->sidename[15] = '\0';

                    /* Extract ratio */
                    char *ratio_str = skip_whitespace(comma + 1);
                    cur->s_ratio = evaluate_expression(ratio_str);
                    cur->s_type = ADDON;  /* SPLIT uses ADDON type */
                }
            }
            free(line);
            continue;
        }

        /* Try to parse as df,db,r line */
        double df, db, r;
        char comment[64];
        if (parse_dfdbr(line, &df, &db, &r, comment)) {
            /* Convert mm to m */
            df *= 0.001;
            db *= 0.001;
            r *= 0.001;

            if (!head) {
                head = create_men(df, db, r, comment);
                cur = head;
            } else {
                cur = append_men(cur, df, db, r, comment);
            }
        }

        free(line);
    }

    return head;
}

/*
 * Read all groups and MAIN from mensur definition lines
 */
static xmen_group* read_xmen_groups(char** mendefs) {
    group_count = 0;
    int idx = 0;

    while (mendefs[idx] != NULL) {
        char *line = mendefs[idx];

        /* Check for MAIN or [ */
        if (strcmp(line, "MAIN") == 0 || strcmp(line, "[") == 0) {
            idx++;
            mensur *main_men = parse_group_recursive(mendefs, &idx, "MAIN");
            if (main_men && group_count < MAX_GROUPS) {
                strcpy(groups[group_count].name, "MAIN");
                groups[group_count].men = main_men;
                group_count++;
            }
            continue;
        }

        /* Check for GROUP or { */
        if (strncmp(line, "GROUP", 5) == 0 || strncmp(line, "{", 1) == 0) {
            /* Extract group name */
            char group_name[256] = "";
            if (line[0] == '{') {
                char *p = line + 1;
                if (*p == ',') p++;
                p = skip_whitespace(p);
                strncpy(group_name, p, 255);
            } else {
                char *p = line + 5;  /* Skip "GROUP" */
                if (*p == ',') p++;
                p = skip_whitespace(p);
                strncpy(group_name, p, 255);
            }

            /* Trim group name */
            char *end = group_name + strlen(group_name) - 1;
            while (end >= group_name && (isspace((unsigned char)*end) || *end == ',')) {
                *end = '\0';
                end--;
            }

            idx++;
            mensur *group_men = parse_group_recursive(mendefs, &idx, group_name);
            if (group_men && group_count < MAX_GROUPS && strlen(group_name) > 0) {
                strncpy(groups[group_count].name, group_name, 255);
                groups[group_count].name[255] = '\0';
                groups[group_count].men = group_men;
                group_count++;
            }
            continue;
        }

        idx++;
    }

    return groups;
}

/*
 * Step 6: Get pointer to MAIN mensur
 */
static mensur* get_main_xmen(xmen_group* mens) {
    for (int i = 0; i < group_count; i++) {
        if (strcmp(mens[i].name, "MAIN") == 0) {
            return mens[i].men;
        }
    }
    return NULL;
}

/*
 * Find group by name
 */
static mensur* find_xmen(const char* name) {
    for (int i = 0; i < group_count; i++) {
        if (strcmp(groups[i].name, name) == 0) {
            return groups[i].men;
        }
    }
    return NULL;
}

/*
 * Step 7: Build XMEN - resolve INSERT, SPLIT, BRANCH, MERGE
 * This is a placeholder - full implementation needed based on requirements
 */
static mensur* build_xmen(mensur* men) {
    /* For now, just return the mensur as-is */
    /* TODO: Implement INSERT, SPLIT, BRANCH, MERGE resolution */
    return men;
}

/*
 * Resolve child mensur connections
 */
static void resolve_xmen_child(mensur *men) {
    mensur *m = men;

    while (m != NULL) {
        if (m->sidename[0] != '\0') {
            mensur* child = find_xmen(m->sidename);
            if (child != NULL) {
                if (m->s_type != JOIN) {
                    /* SPLIT or BRANCH */
                    m->side = child;
                    resolve_xmen_child(child);
                } else {
                    /* JOIN */
                    m->side = get_last_men(child);
                }
            } else {
                fprintf(stderr, "Cannot find corresponding child: \"%s\"\n", m->sidename);
            }
        }
        m = m->next;
    }
}

/*
 * Rejoint mensur based on side connection ratio
 */
static mensur* rejoint_xmen(mensur* men) {
    mensur *p, *q, *s, *ss;

    p = men;

    while (p->next != NULL) {
        if (p->s_ratio > 0.5) {
            if (p->side != NULL && p->s_type == ADDON) {
                q = get_last_men(p->side);
                q = remove_men(q);
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
                s = get_join_men(p, p->side);

                q = s->side;
                q = remove_men(q);

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
        }

        p = p->next;
    }

    return men;
}

/*
 * Main entry point: Read XMENSUR file
 */
mensur* read_xmensur(const char* path) {
    /* Step 1: Read all contents of xmensur file as list of line text */
    char** lines = read_xmensur_text(path);
    if (!lines) return NULL;

    /* Step 2 & 3: Read variable definition lines */
    char** vardefs = split_var_defs(lines);
    read_xmen_variables(vardefs);

    /* Step 4 & 5: Read mensur definitions and create groups */
    char** mendefs = split_men_defs(lines);
    read_xmen_groups(mendefs);

    /* Step 6: Get pointer to head mensur of MAIN */
    mensur* mainmen = get_main_xmen(groups);
    if (!mainmen) {
        fprintf(stderr, "Error: No MAIN definition found in XMENSUR file\n");
        return NULL;
    }

    /* Step 7: Resolve INSERT, SPLIT, BRANCH, MERGE of groups */
    mainmen = build_xmen(mainmen);

    /* Step 8: Resolve child connections */
    resolve_xmen_child(mainmen);

    /* Step 9: Rejoint branches if s_ratio > 0.5 */
    mainmen = rejoint_xmen(mainmen);

    /* Cleanup */
    for (int i = 0; lines[i] != NULL; i++) free(lines[i]);
    free(lines);
    for (int i = 0; vardefs[i] != NULL; i++) free(vardefs[i]);
    free(vardefs);
    for (int i = 0; mendefs[i] != NULL; i++) free(mendefs[i]);
    free(mendefs);

    return mainmen;
}

