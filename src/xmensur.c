/*
 * xmensur.c - XMENSUR format parser
 *
 * XMENSUR format uses:
 * - # for comments (like Python)
 * - [] or MAIN/END_MAIN for main bore
 * - {} or GROUP/END_GROUP for sub-mensur groups
 * - Python-style variable assignments: var = value
 * - OPEN_END, CLOSED_END keywords
 * - <, >, | for BRANCH, MERGE, SPLIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>
#include "zmensur.h"
#include "xmensur.h"
#include "kutils.h"

#define MAX_LINE 1024
#define MAX_VARS 64
#define MAX_GROUPS 32

/* Variable storage */
typedef struct {
    char name[64];
    double value;
} xmen_var;

static xmen_var variables[MAX_VARS];
static int var_count = 0;

/* Group storage */
typedef struct {
    char name[256];
    mensur *men;
} xmen_group;

static xmen_group groups[MAX_GROUPS];
static int group_count = 0;

/* Forward declarations */
static char* skip_whitespace(char *p);
static char* remove_xmen_comment(char *line);
static int parse_variable(char *line);
static double evaluate_expression(char *expr);
static mensur* parse_mensur_section(FILE *fp, const char *group_name, int *in_group);

/*
 * Skip whitespace characters
 */
static char* skip_whitespace(char *p) {
    while (*p && isspace(*p)) p++;
    return p;
}

/*
 * Remove comments (# to end of line)
 */
static char* remove_xmen_comment(char *line) {
    char *p = strchr(line, '#');
    if (p) *p = '\0';
    return line;
}

/*
 * Parse variable assignment: var = value
 * Returns 1 if line was a variable assignment, 0 otherwise
 */
static int parse_variable(char *line) {
    char *eq = strchr(line, '=');
    if (!eq) return 0;

    *eq = '\0';
    char *name = skip_whitespace(line);
    char *value_str = skip_whitespace(eq + 1);

    /* Remove trailing whitespace from name */
    char *end = name + strlen(name) - 1;
    while (end > name && isspace(*end)) *end-- = '\0';

    /* Remove trailing comma if present */
    end = value_str + strlen(value_str) - 1;
    while (end > value_str && (*end == ',' || isspace(*end))) *end-- = '\0';

    if (var_count < MAX_VARS) {
        strncpy(variables[var_count].name, name, sizeof(variables[var_count].name) - 1);
        variables[var_count].value = evaluate_expression(value_str);
        if (getenv("DEBUG_XMENSUR")) {
            fprintf(stderr, "XMENSUR VAR: %s = %s = %.6f\n", name, value_str, variables[var_count].value);
        }
        var_count++;
        return 1;
    }

    return 0;
}

/*
 * Evaluate simple arithmetic expression with variables
 * Supports: +, -, *, /, numbers, variables
 * Uses a simple recursive descent parser with operator precedence
 */
static double evaluate_term(char **p);
static double evaluate_factor(char **p);

static double evaluate_expression(char *expr) {
    char *p = expr;
    return evaluate_term(&p);
}

static double evaluate_term(char **pp) {
    double result = evaluate_factor(pp);
    char *p = skip_whitespace(*pp);

    while (*p == '+' || *p == '-') {
        char op = *p++;
        *pp = p;
        double right = evaluate_factor(pp);
        if (op == '+') result += right;
        else result -= right;
        p = skip_whitespace(*pp);
    }
    *pp = p;
    return result;
}

static double evaluate_factor(char **pp) {
    double result;
    char *p = skip_whitespace(*pp);

    /* Check if it's a variable */
    if (isalpha(*p) || *p == '_') {
        char varname[64];
        int i = 0;
        while ((isalnum(*p) || *p == '_') && i < 63) {
            varname[i++] = *p++;
        }
        varname[i] = '\0';

        /* Look up variable */
        result = 0.0;
        for (i = 0; i < var_count; i++) {
            if (strcmp(variables[i].name, varname) == 0) {
                result = variables[i].value;
                break;
            }
        }
    } else {
        /* Parse as number */
        result = strtod(p, &p);
    }

    /* Handle * and / */
    p = skip_whitespace(p);
    while (*p == '*' || *p == '/') {
        char op = *p++;
        p = skip_whitespace(p);
        double right;
        if (isalpha(*p) || *p == '_') {
            char varname[64];
            int i = 0;
            while ((isalnum(*p) || *p == '_') && i < 63) {
                varname[i++] = *p++;
            }
            varname[i] = '\0';
            right = 0.0;
            for (i = 0; i < var_count; i++) {
                if (strcmp(variables[i].name, varname) == 0) {
                    right = variables[i].value;
                    break;
                }
            }
        } else {
            right = strtod(p, &p);
        }

        if (op == '*') result *= right;
        else if (right != 0.0) result /= right;

        p = skip_whitespace(p);
    }

    *pp = p;
    return result;
}

/*
 * Parse df,db,r values from line
 * Returns 1 on success, 0 on failure
 */
static int parse_dfdbr(char *line, double *df, double *db, double *r, char *comment) {
    char *tokens[4];
    int token_count = 0;
    char *p = line;
    char *start;

    /* Split by commas */
    while (*p && token_count < 4) {
        while (*p && isspace(*p)) p++;
        if (!*p) break;

        start = p;
        while (*p && *p != ',') p++;

        size_t len = p - start;
        tokens[token_count] = malloc(len + 1);
        strncpy(tokens[token_count], start, len);
        tokens[token_count][len] = '\0';

        /* Trim trailing whitespace */
        char *end = tokens[token_count] + strlen(tokens[token_count]) - 1;
        while (end >= tokens[token_count] && isspace(*end)) *end-- = '\0';

        token_count++;
        if (*p == ',') p++;
    }

    if (token_count < 3) {
        for (int i = 0; i < token_count; i++) free(tokens[i]);
        return 0;
    }

    *df = evaluate_expression(tokens[0]);
    *db = evaluate_expression(tokens[1]);
    *r = evaluate_expression(tokens[2]);

    if (token_count > 3 && comment) {
        strncpy(comment, tokens[3], 63);
        comment[63] = '\0';
    } else if (comment) {
        comment[0] = '\0';
    }

    for (int i = 0; i < token_count; i++) free(tokens[i]);
    return 1;
}

/*
 * Parse a mensur section (between brackets or MAIN/END_MAIN)
 */
static mensur* parse_mensur_section(FILE *fp, const char *group_name, int *in_group) {
    char line[MAX_LINE];
    mensur *head = NULL, *cur = NULL;
    double df, db, r;
    char comment[64];

    while (fgets(line, sizeof(line), fp)) {
        remove_xmen_comment(line);
        char *p = skip_whitespace(line);

        /* Skip empty lines */
        if (!*p || *p == '\n') continue;

        /* Remove trailing newline */
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';

        if (getenv("DEBUG_XMENSUR_VERBOSE")) {
            fprintf(stderr, "  [%s] Read: '%s'\n", group_name, p);
        }

        /* Check for end markers */
        if (strcmp(p, "]") == 0 || strcmp(p, "END_MAIN") == 0 ||
            strcmp(p, "}") == 0 || strcmp(p, "END_GROUP") == 0) {
            *in_group = 0;
            break;
        }

        /* Check for variable assignment */
        if (strchr(p, '=')) {
            parse_variable(p);
            continue;
        }

        /* Check for special keywords */
        if (strcmp(p, "OPEN_END") == 0) {
            if (getenv("DEBUG_XMENSUR_VERBOSE")) {
                fprintf(stderr, "  [%s] Matched OPEN_END, cur=%p\n", group_name, (void*)cur);
            }
            if (cur) {
                if (getenv("DEBUG_XMENSUR")) {
                    fprintf(stderr, "XMENSUR [%s]: df=%.4f, db=0.0000, r=0.0000, comment='OPEN_END'\n",
                            group_name, cur->db);
                }
                cur = append_men(cur, cur->db, 0, 0, "");
            }
            continue;
        }

        if (strcmp(p, "CLOSED_END") == 0) {
            if (cur) {
                if (getenv("DEBUG_XMENSUR")) {
                    fprintf(stderr, "XMENSUR [%s]: df=0.0000, db=0.0000, r=0.0000, comment='CLOSED_END'\n",
                            group_name);
                }
                cur = append_men(cur, 0, 0, 0, "");
            }
            continue;
        }

        /* Check for branch/merge/split directives */
        if (*p == '<' || *p == '>' || *p == '|' ||
            strncmp(p, "BRANCH", 6) == 0 || strncmp(p, "MERGE", 5) == 0 ||
            strncmp(p, "SPLIT", 5) == 0) {

            char directive = *p;
            if (strncmp(p, "BRANCH", 6) == 0) directive = '>';  /* BRANCH -> SPLIT */
            else if (strncmp(p, "MERGE", 5) == 0) directive = '<';  /* MERGE -> JOIN */
            else if (strncmp(p, "SPLIT", 5) == 0) directive = '|';

            /* Skip directive character and comma */
            p++;
            if (*p == ',') p++;
            p = skip_whitespace(p);

            /* Parse name and ratio */
            char name[256];
            double ratio = 1.0;
            char *comma = strchr(p, ',');
            if (comma) {
                size_t len = comma - p;
                if (len > 255) len = 255;
                strncpy(name, p, len);
                name[len] = '\0';
                /* Trim whitespace */
                char *end = name + strlen(name) - 1;
                while (end >= name && isspace(*end)) *end-- = '\0';

                ratio = evaluate_expression(skip_whitespace(comma + 1));
            } else {
                strncpy(name, p, 255);
                name[255] = '\0';
                /* Trim whitespace and trailing comma */
                char *end = name + strlen(name) - 1;
                while (end >= name && (isspace(*end) || *end == ',')) *end-- = '\0';
            }

            /* Create marker mensur with side branch reference */
            if (cur) {
                mensur *marker = append_men(cur, cur->db, cur->db, 0, "");
                strncpy(marker->sidename, name, sizeof(marker->sidename) - 1);
                marker->s_ratio = ratio;

                if (directive == '>') {
                    marker->s_type = SPLIT;  /* BRANCH start (>) */
                } else if (directive == '<') {
                    marker->s_type = JOIN;   /* MERGE end (<) */
                } else if (directive == '|') {
                    marker->s_type = TONEHOLE;  /* SPLIT/tonehole */
                }

                cur = marker;
            }
            continue;
        }

        /* Check for INSERT directive */
        if (*p == '@' || strncmp(p, "INSERT", 6) == 0) {
            /* Similar to branch handling */
            if (*p == '@') p++;
            else p += 6;

            if (*p == ',') p++;
            p = skip_whitespace(p);

            char name[256];
            strncpy(name, p, 255);
            name[255] = '\0';
            char *end = name + strlen(name) - 1;
            while (end >= name && (isspace(*end) || *end == ',')) *end-- = '\0';

            /* Insert will be resolved later */
            if (cur) {
                mensur *marker = append_men(cur, cur->db, cur->db, 0, "");
                strncpy(marker->sidename, name, sizeof(marker->sidename) - 1);
                marker->s_type = ADDON;
                cur = marker;
            }
            continue;
        }

        /* Try to parse as df,db,r line */
        if (parse_dfdbr(p, &df, &db, &r, comment)) {
            /* Convert mm to m */
            df *= 0.001;
            db *= 0.001;
            r *= 0.001;

            if (getenv("DEBUG_XMENSUR")) {
                fprintf(stderr, "XMENSUR [%s]: df=%.4f, db=%.4f, r=%.4f, comment='%s'\n",
                        group_name, df, db, r, comment);
            }

            if (!head) {
                head = create_men(df, db, r, comment);
                cur = head;
            } else {
                cur = append_men(cur, df, db, r, comment);
            }
        }
    }

    return head;
}

/*
 * Main function to read XMENSUR format file
 */
mensur* read_xmensur(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open XMENSUR file: %s\n", path);
        return NULL;
    }

    /* Reset global state */
    var_count = 0;
    group_count = 0;

    char line[MAX_LINE];
    mensur *main_men = NULL;
    int in_group = 0;
    char current_group[256] = "";

    while (fgets(line, sizeof(line), fp)) {
        remove_xmen_comment(line);
        char *p = skip_whitespace(line);

        /* Skip empty lines */
        if (!*p || *p == '\n') continue;

        /* Remove trailing newline */
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';

        /* Check for variable assignment */
        if (strchr(p, '=')) {
            parse_variable(p);
            continue;
        }

        /* Check for main section start */
        if (*p == '[' || strcmp(p, "MAIN") == 0) {
            in_group = 1;
            main_men = parse_mensur_section(fp, "MAIN", &in_group);
            continue;
        }

        /* Check for group section start */
        if (*p == '{' || strncmp(p, "GROUP", 5) == 0) {
            char group_name[256] = "";

            if (*p == '{') {
                p++;
                if (*p == ',') p++;
                p = skip_whitespace(p);
                strncpy(group_name, p, 255);
            } else {
                p += 5;  /* Skip "GROUP" */
                if (*p == ',') p++;
                p = skip_whitespace(p);
                strncpy(group_name, p, 255);
            }

            /* Trim whitespace */
            char *end = group_name + strlen(group_name) - 1;
            while (end >= group_name && (isspace(*end) || *end == ',')) *end-- = '\0';

            in_group = 1;
            mensur *group_men = parse_mensur_section(fp, group_name, &in_group);

            if (group_count < MAX_GROUPS && group_men) {
                strncpy(groups[group_count].name, group_name, 255);
                groups[group_count].men = group_men;
                group_count++;
            }
            continue;
        }
    }

    fclose(fp);

    /* Resolve side branches and insertions */
    if (main_men) {
        mensur *m = main_men;
        while (m) {
            if (m->sidename[0] != '\0') {
                /* Find the group */
                for (int i = 0; i < group_count; i++) {
                    if (strcmp(groups[i].name, m->sidename) == 0) {
                        if (m->s_type == ADDON) {
                            /* Insert group into main chain */
                            mensur *group_last = get_last_men(groups[i].men);
                            if (group_last && m->next) {
                                group_last->next = m->next;
                                m->next->prev = group_last;
                            }
                            m->next = groups[i].men;
                            groups[i].men->prev = m;
                        } else {
                            /* Attach as side branch */
                            m->side = groups[i].men;
                        }
                        break;
                    }
                }
            }
            m = m->next;
        }

        /* Rejoint branches based on s_ratio */
        main_men = rejoint_men(main_men);
    }

    return get_first_men(main_men);
}
