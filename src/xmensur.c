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
 * Utility: case-insensitive string comparison
 */
static int strcasecmp_xmen(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = tolower((unsigned char)*s1);
        char c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/*
 * Utility: case-insensitive string comparison with length limit
 */
static int strncasecmp_xmen(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && *s2) {
        char c1 = tolower((unsigned char)*s1);
        char c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

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
    int line_capacity = 1000;  /* Initial capacity */

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

    /* Allocate initial lines array */
    lines = (char**)malloc(line_capacity * sizeof(char*));
    if (!lines) {
        fprintf(stderr, "Cannot allocate memory for lines\n");
        free(readbuffer);
        return NULL;
    }

    /* Split into lines, trim, and skip blank/comment lines */
    char *line = strtok(readbuffer, "\n");
    while (line != NULL) {
        char *trimmed = trim_line(strdup(line));
        if (strlen(trimmed) > 0) {
            /* Expand array if needed */
            if (line_count >= line_capacity - 1) {
                line_capacity *= 2;
                char **new_lines = (char**)realloc(lines, line_capacity * sizeof(char*));
                if (!new_lines) {
                    fprintf(stderr, "Cannot reallocate memory for lines\n");
                    for (int i = 0; i < line_count; i++) free(lines[i]);
                    free(lines);
                    free(readbuffer);
                    return NULL;
                }
                lines = new_lines;
            }
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
    int capacity = 100;
    char **vardefs = (char**)malloc(capacity * sizeof(char*));
    int var_idx = 0;

    if (!vardefs) {
        fprintf(stderr, "Cannot allocate memory for vardefs\n");
        return NULL;
    }

    for (int i = 0; lines[i] != NULL; i++) {
        /* Check if line contains '=' and is not a marker */
        if (strchr(lines[i], '=') &&
            strncmp(lines[i], "MAIN", 4) != 0 &&
            strncmp(lines[i], "GROUP", 5) != 0 &&
            lines[i][0] != '[' && lines[i][0] != '{') {

            /* Expand array if needed */
            if (var_idx >= capacity - 1) {
                capacity *= 2;
                char **new_vardefs = (char**)realloc(vardefs, capacity * sizeof(char*));
                if (!new_vardefs) {
                    fprintf(stderr, "Cannot reallocate memory for vardefs\n");
                    for (int j = 0; j < var_idx; j++) free(vardefs[j]);
                    free(vardefs);
                    return NULL;
                }
                vardefs = new_vardefs;
            }
            vardefs[var_idx++] = strdup(lines[i]);
        }
    }
    vardefs[var_idx] = NULL;

    return vardefs;
}

/*
 * Check if a variable name already exists
 */
static int variable_exists(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Step 3: Read variable definitions
 * Note: No whitespace handling needed - already trimmed by read_xmensur_text
 * Returns NULL if variable count exceeds MAX_VARS or if duplicate variables are found
 */
static xmen_var* read_xmen_variables(char** vardefs) {
    var_count = 0;

    for (int i = 0; vardefs[i] != NULL; i++) {
        char *line = strdup(vardefs[i]);
        char *eq = strchr(line, '=');
        if (!eq) {
            free(line);
            continue;
        }

        *eq = '\0';
        char *name = line;
        char *value_str = eq + 1;

        /* Only need to trim internal spaces around = sign */
        while (*name && name[strlen(name)-1] == ' ') name[strlen(name)-1] = '\0';
        while (*value_str == ' ') value_str++;

        if (strlen(name) > 0) {
            /* Check for duplicate variable name */
            if (variable_exists(name)) {
                fprintf(stderr, "Error: Duplicate variable definition: '%s'\n", name);
                free(line);
                return NULL;
            }

            if (var_count >= MAX_VARS) {
                fprintf(stderr, "Error: Number of variables (%d) exceeds maximum limit (%d)\n",
                        var_count + 1, MAX_VARS);
                free(line);
                return NULL;
            }
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
    int capacity = 1000;
    char **mendefs = (char**)malloc(capacity * sizeof(char*));
    int men_idx = 0;

    if (!mendefs) {
        fprintf(stderr, "Cannot allocate memory for mendefs\n");
        return NULL;
    }

    for (int i = 0; lines[i] != NULL; i++) {
        /* Skip variable definitions */
        if (strchr(lines[i], '=') &&
            strncmp(lines[i], "MAIN", 4) != 0 &&
            strncmp(lines[i], "GROUP", 5) != 0 &&
            lines[i][0] != '[' && lines[i][0] != '{') {
            continue;
        }

        /* Expand array if needed */
        if (men_idx >= capacity - 1) {
            capacity *= 2;
            char **new_mendefs = (char**)realloc(mendefs, capacity * sizeof(char*));
            if (!new_mendefs) {
                fprintf(stderr, "Cannot reallocate memory for mendefs\n");
                for (int j = 0; j < men_idx; j++) free(mendefs[j]);
                free(mendefs);
                return NULL;
            }
            mendefs = new_mendefs;
        }
        mendefs[men_idx++] = strdup(lines[i]);
    }
    mendefs[men_idx] = NULL;

    return mendefs;
}

/* Forward declaration */
static mensur* parse_group_recursive(char** lines, int *idx, const char *group_name, int *error);

/*
 * Check if a line looks like an unrecognized keyword
 * Returns 1 if it looks like a keyword (starts with uppercase letter and contains no comma)
 */
static int is_unrecognized_keyword(const char *line) {
    if (!line || strlen(line) == 0) return 0;

    /* Check if starts with uppercase letter or special character */
    if (isupper((unsigned char)line[0]) || line[0] == '[' || line[0] == ']' ||
        line[0] == '{' || line[0] == '}' || line[0] == '>' ||
        line[0] == '<' || line[0] == '|') {
        /* If it contains a comma followed by numbers, it might be a cell definition */
        const char *comma = strchr(line, ',');
        if (comma) {
            /* Check if there are numbers around the comma (could be df,db,r) */
            const char *p = line;
            int has_digit_before = 0;
            while (p < comma && !has_digit_before) {
                if (isdigit((unsigned char)*p)) has_digit_before = 1;
                p++;
            }
            if (has_digit_before) return 0;  /* Looks like a cell definition */
        }
        return 1;  /* Looks like a keyword */
    }
    return 0;
}

/*
 * Parse df,db,r line
 */
static int parse_xmen_cell(char *line, double *df, double *db, double *r, char *comment) {
    char *tokens[4];
    int token_count = 0;

    /* Tokenize by comma */
    char *p = line;
    char *start = p;
    while (*p && token_count < 4) {
        if (*p == ',') {
            *p = '\0';
            tokens[token_count++] = start;
            start = p + 1;
        }
        p++;
    }
    if (token_count < 4 && *start) {
        tokens[token_count++] = start;
    }

    if (token_count < 3) return 0;

    *df = evaluate_expression(tokens[0]);
    *db = evaluate_expression(tokens[1]);
    *r = evaluate_expression(tokens[2]);

    if (comment) {
        if (token_count > 3 && tokens[3]) {
            strncpy(comment, tokens[3], 63);
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
 * Returns NULL on error, sets *error to 1
 */
static mensur* parse_group_recursive(char** lines, int *idx, const char *group_name, int *error) {
    mensur *head = NULL, *cur = NULL;
    int depth = 1;  /* We're inside a group */
    int is_main = (strcasecmp_xmen(group_name, "MAIN") == 0);

    while (lines[*idx] != NULL) {
        char *line = strdup(lines[*idx]);
        (*idx)++;

        /* Handle END_GROUP or } or END_MAIN or ] */
        if (strcasecmp_xmen(line, "END_GROUP") == 0 || strcmp(line, "}") == 0 ||
            strcmp(line, "]") == 0 || strcasecmp_xmen(line, "END_MAIN") == 0) {

            /* Check for mismatched closing markers */
            int is_end_main = (strcasecmp_xmen(line, "END_MAIN") == 0 || strcmp(line, "]") == 0);
            int is_end_group = (strcasecmp_xmen(line, "END_GROUP") == 0 || strcmp(line, "}") == 0);

            if (is_main && is_end_group && depth == 1) {
                fprintf(stderr, "Error: Found END_GROUP/} but expected END_MAIN/] for MAIN block\n");
                free(line);
                *error = 1;
                return NULL;
            }
            if (!is_main && is_end_main && depth == 1) {
                fprintf(stderr, "Error: Found END_MAIN/] but expected END_GROUP/} for GROUP '%s'\n", group_name);
                free(line);
                *error = 1;
                return NULL;
            }

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

        /* Handle nested MAIN or [ */
        if (strcasecmp_xmen(line, "MAIN") == 0 || strcmp(line, "[") == 0) {
            depth++;
            free(line);
            continue;
        }

        /* Handle nested GROUP or { */
        if (strncasecmp_xmen(line, "GROUP", 5) == 0 || strncmp(line, "{", 1) == 0) {
            depth++;
            free(line);
            continue;
        }

        /* Handle OPEN_END */
        if (strcasecmp_xmen(line, "OPEN_END") == 0) {
            if (cur) {
                cur = append_men(cur, cur->db, 0, 0, "");
            }
            free(line);
            continue;
        }

        /* Handle CLOSED_END */
        if (strcasecmp_xmen(line, "CLOSED_END") == 0) {
            if (cur) {
                cur = append_men(cur, 0, 0, 0, "");
            }
            free(line);
            continue;
        }

        /* Handle BRANCH marker: BRANCH, groupname, ratio */
        if (strncasecmp_xmen(line, "BRANCH", 6) == 0 || line[0] == '>') {
            if (cur) {
                char *p = (line[0] == '>') ? line + 1 : line + 6;
                if (*p == ',') p++;

                /* Extract group name */
                char *comma = strchr(p, ',');
                if (comma) {
                    *comma = '\0';
                    strncpy(cur->sidename, p, 15);
                    cur->sidename[15] = '\0';

                    /* Extract ratio */
                    char *ratio_str = comma + 1;
                    cur->s_ratio = evaluate_expression(ratio_str);
                    cur->s_type = SPLIT;  /* BRANCH uses SPLIT type */
                }
            }
            free(line);
            continue;
        }

        /* Handle MERGE marker: MERGE, groupname, ratio */
        if (strncasecmp_xmen(line, "MERGE", 5) == 0 || line[0] == '<') {
            if (cur) {
                char *p = (line[0] == '<') ? line + 1 : line + 5;
                if (*p == ',') p++;

                /* Extract group name */
                char *comma = strchr(p, ',');
                if (comma) {
                    *comma = '\0';
                    strncpy(cur->sidename, p, 15);
                    cur->sidename[15] = '\0';

                    /* Extract ratio */
                    char *ratio_str = comma + 1;
                    cur->s_ratio = evaluate_expression(ratio_str);
                    cur->s_type = JOIN;  /* MERGE uses JOIN type */
                }
            }
            free(line);
            continue;
        }

        /* Handle SPLIT marker: SPLIT, groupname, ratio or |,groupname,ratio */
        if (strncasecmp_xmen(line, "SPLIT", 5) == 0 || line[0] == '|') {
            if (cur) {
                char *p = (line[0] == '|') ? line + 1 : line + 5;
                if (*p == ',') p++;

                /* Extract group name */
                char *comma = strchr(p, ',');
                if (comma) {
                    *comma = '\0';
                    strncpy(cur->sidename, p, 15);
                    cur->sidename[15] = '\0';

                    /* Extract ratio */
                    char *ratio_str = comma + 1;
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
        if (parse_xmen_cell(line, &df, &db, &r, comment)) {
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
        } else {
            /* If it's not a valid cell and looks like a keyword, report error */
            if (is_unrecognized_keyword(line)) {
                fprintf(stderr, "Error: Unrecognized keyword: '%s'\n", line);
                free(line);
                *error = 1;
                return NULL;
            }
            /* Otherwise, silently ignore (could be empty line or malformed cell) */
        }

        free(line);
    }

    /* Check if we exited the loop without finding the closing marker */
    if (depth > 0) {
        if (is_main) {
            fprintf(stderr, "Error: Missing END_MAIN/] for MAIN block\n");
        } else {
            fprintf(stderr, "Error: Missing END_GROUP/} for GROUP '%s'\n", group_name);
        }
        *error = 1;
        return NULL;
    }

    return head;
}

/*
 * Check if a group name already exists (case-insensitive)
 */
static int group_exists(const char* name) {
    for (int i = 0; i < group_count; i++) {
        if (strcasecmp_xmen(groups[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Read all groups and MAIN from mensur definition lines
 * Returns NULL on error
 */
static xmen_group* read_xmen_groups(char** mendefs) {
    group_count = 0;
    int idx = 0;
    int error = 0;

    while (mendefs[idx] != NULL) {
        char *line = mendefs[idx];

        /* Check for MAIN or [ */
        if (strcasecmp_xmen(line, "MAIN") == 0 || strcmp(line, "[") == 0) {
            /* Check for duplicate MAIN definition */
            if (group_exists("MAIN")) {
                fprintf(stderr, "Error: Duplicate MAIN block definition\n");
                return NULL;
            }

            if (group_count >= MAX_GROUPS) {
                fprintf(stderr, "Error: Number of groups (%d) exceeds maximum limit (%d)\n",
                        group_count + 1, MAX_GROUPS);
                return NULL;
            }
            idx++;
            mensur *main_men = parse_group_recursive(mendefs, &idx, "MAIN", &error);
            if (error) {
                return NULL;
            }
            if (main_men) {
                strcpy(groups[group_count].name, "MAIN");
                groups[group_count].men = main_men;
                group_count++;
            }
            continue;
        }

        /* Check for GROUP or { */
        if (strncasecmp_xmen(line, "GROUP", 5) == 0 || strncmp(line, "{", 1) == 0) {
            if (group_count >= MAX_GROUPS) {
                fprintf(stderr, "Error: Number of groups (%d) exceeds maximum limit (%d)\n",
                        group_count + 1, MAX_GROUPS);
                return NULL;
            }

            /* Extract group name */
            char group_name[256] = "";
            if (line[0] == '{') {
                char *p = line + 1;
                if (*p == ',') p++;
                strncpy(group_name, p, 255);
            } else {
                char *p = line + 5;  /* Skip "GROUP" */
                if (*p == ',') p++;
                strncpy(group_name, p, 255);
            }

            /* Trim group name */
            char *end = group_name + strlen(group_name) - 1;
            while (end >= group_name && (isspace((unsigned char)*end) || *end == ',')) {
                *end = '\0';
                end--;
            }

            /* Check for duplicate group name */
            if (strlen(group_name) > 0 && group_exists(group_name)) {
                fprintf(stderr, "Error: Duplicate group definition: '%s'\n", group_name);
                return NULL;
            }

            idx++;
            mensur *group_men = parse_group_recursive(mendefs, &idx, group_name, &error);
            if (error) {
                return NULL;
            }
            if (group_men && strlen(group_name) > 0) {
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
 * Step 6: Get pointer to MAIN mensur (case-insensitive)
 */
static mensur* get_main_xmen(xmen_group* mens) {
    for (int i = 0; i < group_count; i++) {
        if (strcasecmp_xmen(mens[i].name, "MAIN") == 0) {
            return mens[i].men;
        }
    }
    return NULL;
}

/*
 * Find group by name (case-insensitive)
 */
static mensur* find_xmen(const char* name) {
    for (int i = 0; i < group_count; i++) {
        if (strcasecmp_xmen(groups[i].name, name) == 0) {
            return groups[i].men;
        }
    }
    return NULL;
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
    xmen_var* parsed_vars = read_xmen_variables(vardefs);
    if (!parsed_vars) {
        fprintf(stderr, "Error: Failed to parse variables (exceeded limit)\n");
        /* Cleanup */
        for (int i = 0; lines[i] != NULL; i++) free(lines[i]);
        free(lines);
        for (int i = 0; vardefs[i] != NULL; i++) free(vardefs[i]);
        free(vardefs);
        return NULL;
    }

    /* Step 4 & 5: Read mensur definitions and create groups */
    char** mendefs = split_men_defs(lines);
    xmen_group* parsed_groups = read_xmen_groups(mendefs);
    if (!parsed_groups) {
        fprintf(stderr, "Error: Failed to parse XMENSUR groups\n");
        /* Cleanup */
        for (int i = 0; lines[i] != NULL; i++) free(lines[i]);
        free(lines);
        for (int i = 0; vardefs[i] != NULL; i++) free(vardefs[i]);
        free(vardefs);
        for (int i = 0; mendefs[i] != NULL; i++) free(mendefs[i]);
        free(mendefs);
        return NULL;
    }

    /* Step 6: Get pointer to head mensur of MAIN */
    mensur* mainmen = get_main_xmen(parsed_groups);
    if (!mainmen) {
        fprintf(stderr, "Error: No MAIN definition found in XMENSUR file\n");
        /* Cleanup */
        for (int i = 0; lines[i] != NULL; i++) free(lines[i]);
        free(lines);
        for (int i = 0; vardefs[i] != NULL; i++) free(vardefs[i]);
        free(vardefs);
        for (int i = 0; mendefs[i] != NULL; i++) free(mendefs[i]);
        free(mendefs);
        return NULL;
    }

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

/*
 * Test function for error handling validation
 * Returns 0 on success, non-zero on failure
 */
int test_xmensur_error_handling(void) {
    int test_count = 0;
    int passed = 0;
    int failed = 0;
    FILE *f;
    mensur *result;

    printf("Testing XMENSUR error handling\n");
    printf("================================\n\n");

    /* Test 1: Duplicate variable definition */
    test_count++;
    printf("Test %d: Duplicate variable definition\n", test_count);
    f = fopen("test_dup_var.xmen", "w");
    fprintf(f, "x = 10\ny = 20\nx = 30\nMAIN\n10,10,100\n10,0,0\nEND_MAIN\n");
    fclose(f);
    result = read_xmensur("test_dup_var.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Test 2: Duplicate group definition */
    test_count++;
    printf("Test %d: Duplicate group definition\n", test_count);
    f = fopen("test_dup_group.xmen", "w");
    fprintf(f, "MAIN\n10,10,100\n10,0,0\nEND_MAIN\n");
    fprintf(f, "GROUP,side\n5,5,50\n5,0,0\nEND_GROUP\n");
    fprintf(f, "GROUP,side\n5,5,50\n5,0,0\nEND_GROUP\n");
    fclose(f);
    result = read_xmensur("test_dup_group.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Test 3: Duplicate MAIN block */
    test_count++;
    printf("Test %d: Duplicate MAIN block\n", test_count);
    f = fopen("test_dup_main.xmen", "w");
    fprintf(f, "MAIN\n10,10,100\n10,0,0\nEND_MAIN\n");
    fprintf(f, "MAIN\n10,10,100\n10,0,0\nEND_MAIN\n");
    fclose(f);
    result = read_xmensur("test_dup_main.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Test 4: Missing END_MAIN */
    test_count++;
    printf("Test %d: Missing END_MAIN\n", test_count);
    f = fopen("test_missing_end.xmen", "w");
    fprintf(f, "MAIN\n10,10,100\n");
    fclose(f);
    result = read_xmensur("test_missing_end.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Test 5: Mismatched MAIN with END_GROUP */
    test_count++;
    printf("Test %d: MAIN with END_GROUP (mismatched)\n", test_count);
    f = fopen("test_mismatch.xmen", "w");
    fprintf(f, "MAIN\n10,10,100\nEND_GROUP\n");
    fclose(f);
    result = read_xmensur("test_mismatch.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Test 6: Valid file (should succeed) */
    test_count++;
    printf("Test %d: Valid XMENSUR file\n", test_count);
    f = fopen("test_valid.xmen", "w");
    fprintf(f, "x = 10\ny = 20\n");
    fprintf(f, "MAIN\n10,10,100\n10,0,0\nEND_MAIN\n");
    fprintf(f, "GROUP,side\n5,5,50\n5,0,0\nEND_GROUP\n");
    fclose(f);
    result = read_xmensur("test_valid.xmen");
    if (result != NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected non-NULL)\n\n");
        failed++;
    }

    /* Test 7: Unrecognized keyword */
    test_count++;
    printf("Test %d: Unrecognized keyword\n", test_count);
    f = fopen("test_unknown_keyword.xmen", "w");
    fprintf(f, "MAIN\n10,10,100\nUNKNOWN_KEYWORD\n10,0,0\nEND_MAIN\n");
    fclose(f);
    result = read_xmensur("test_unknown_keyword.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Test 8: Unrecognized keyword with comma */
    test_count++;
    printf("Test %d: Unrecognized keyword with comma\n", test_count);
    f = fopen("test_unknown_with_comma.xmen", "w");
    fprintf(f, "MAIN\n10,10,100\nBAD_COMMAND,param\n10,0,0\nEND_MAIN\n");
    fclose(f);
    result = read_xmensur("test_unknown_with_comma.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Test 9: Mixed case valid keywords (should succeed now) */
    test_count++;
    printf("Test %d: Mixed case valid keywords (case-insensitive)\n", test_count);
    f = fopen("test_mixed_case.xmen", "w");
    fprintf(f, "Main\n10,10,100\nOpen_End\nEnd_Main\n");
    fclose(f);
    result = read_xmensur("test_mixed_case.xmen");
    if (result != NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected non-NULL)\n\n");
        failed++;
    }

    /* Test 10: All lowercase keywords should work */
    test_count++;
    printf("Test %d: All lowercase keywords\n", test_count);
    f = fopen("test_all_lowercase.xmen", "w");
    fprintf(f, "main\n10,10,100\nopen_end\nend_main\n");
    fclose(f);
    result = read_xmensur("test_all_lowercase.xmen");
    if (result != NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected non-NULL)\n\n");
        failed++;
    }

    /* Test 11: Unknown keyword should still fail */
    test_count++;
    printf("Test %d: Unknown keyword\n", test_count);
    f = fopen("test_truly_unknown.xmen", "w");
    fprintf(f, "MAIN\n10,10,100\nBAD_KEYWORD\n10,0,0\nEND_MAIN\n");
    fclose(f);
    result = read_xmensur("test_truly_unknown.xmen");
    if (result == NULL) {
        printf("  ✓ PASSED\n\n");
        passed++;
    } else {
        printf("  ✗ FAILED (expected NULL)\n\n");
        failed++;
    }

    /* Cleanup test files */
    remove("test_dup_var.xmen");
    remove("test_dup_group.xmen");
    remove("test_dup_main.xmen");
    remove("test_missing_end.xmen");
    remove("test_mismatch.xmen");
    remove("test_valid.xmen");
    remove("test_unknown_keyword.xmen");
    remove("test_unknown_with_comma.xmen");
    remove("test_mixed_case.xmen");
    remove("test_all_lowercase.xmen");
    remove("test_truly_unknown.xmen");

    printf("================================\n");
    printf("Tests completed: %d total, %d passed, %d failed\n", test_count, passed, failed);

    return (failed == 0) ? 0 : 1;
}

