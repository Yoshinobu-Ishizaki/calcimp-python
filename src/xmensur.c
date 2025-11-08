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
#define MAX_GROUPS 32
#define MAX_LINE 1024

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
static char* skip_whitespace(char *s);
static char* trim_line(char *line);
static double evaluate_expression(char *expr);
static int parse_variable(char *line);
static void read_xmen_variables(const char *buffer);
static void read_xmen_groups(const char *buffer);
static mensur* parse_group_content(const char **lines, int *line_idx, int max_lines);
static mensur* build_xmen(const char *buffer);
static void resolve_xmen_child(mensur *men);

/*
 * Skip leading whitespace
 */
static char* skip_whitespace(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/*
 * Trim line: remove comments and trailing whitespace
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
 * Parse variable assignment: var = value
 */
static int parse_variable(char *line) {
    char *eq = strchr(line, '=');
    if (!eq) return 0;

    *eq = '\0';
    char *name = skip_whitespace(line);
    char *value_str = skip_whitespace(eq + 1);

    /* Remove trailing whitespace */
    char *end = name + strlen(name) - 1;
    while (end > name && isspace((unsigned char)*end)) *end-- = '\0';

    end = value_str + strlen(value_str) - 1;
    while (end > value_str && (*end == ',' || isspace((unsigned char)*end))) *end-- = '\0';

    if (var_count < MAX_VARS && strlen(name) > 0) {
        strncpy(variables[var_count].name, name, sizeof(variables[var_count].name) - 1);
        variables[var_count].name[63] = '\0';
        variables[var_count].value = evaluate_expression(value_str);
        var_count++;
        return 1;
    }

    return 0;
}

/*
 * Parse df,db,r,comment line
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
 * Phase 1: Read and parse all variable definitions
 */
static void read_xmen_variables(const char *buffer) {
    var_count = 0;

    char *buf = strdup(buffer);
    char *line = strtok(buf, "\n");

    while (line != NULL) {
        line = trim_line(line);

        if (strlen(line) == 0) {
            line = strtok(NULL, "\n");
            continue;
        }

        /* Stop when we hit MAIN or GROUP */
        if (strcmp(line, "MAIN") == 0 || strcmp(line, "[") == 0 ||
            strncmp(line, "GROUP", 5) == 0 || line[0] == '{') {
            break;
        }

        /* Parse variable */
        if (strchr(line, '=')) {
            char *line_copy = strdup(line);
            parse_variable(line_copy);
            free(line_copy);
        }

        line = strtok(NULL, "\n");
    }

    free(buf);
}

/*
 * Parse content of a GROUP and return mensur chain
 */
static mensur* parse_group_content(const char **lines, int *line_idx, int max_lines) {
    mensur *head = NULL, *cur = NULL;

    while (*line_idx < max_lines) {
        char *line = strdup(lines[*line_idx]);
        (*line_idx)++;

        line = trim_line(line);

        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        /* Check for end markers */
        if (strcmp(line, "END_GROUP") == 0 || strcmp(line, "}") == 0) {
            free(line);
            break;
        }

        /* Check for OPEN_END */
        if (strcmp(line, "OPEN_END") == 0) {
            if (cur) {
                cur = append_men(cur, cur->db, 0, 0, "");
            }
            free(line);
            continue;
        }

        /* Check for CLOSED_END */
        if (strcmp(line, "CLOSED_END") == 0) {
            if (cur) {
                cur = append_men(cur, 0, 0, 0, "");
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
 * Phase 2: Read and store all GROUP definitions
 */
static void read_xmen_groups(const char *buffer) {
    group_count = 0;

    /* Split buffer into lines */
    char *buf = strdup(buffer);
    char *all_lines[10000];
    int line_count = 0;

    char *line = strtok(buf, "\n");
    while (line != NULL && line_count < 10000) {
        all_lines[line_count++] = strdup(line);
        line = strtok(NULL, "\n");
    }
    free(buf);

    /* Find and parse GROUP definitions outside MAIN */
    int in_main = 0;
    int i = 0;

    while (i < line_count) {
        char *line = trim_line(strdup(all_lines[i]));
        i++;

        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        /* Track MAIN section */
        if (strcmp(line, "MAIN") == 0 || strcmp(line, "[") == 0) {
            in_main = 1;
            free(line);
            continue;
        }
        if (strcmp(line, "END_MAIN") == 0 || strcmp(line, "]") == 0) {
            in_main = 0;
            free(line);
            continue;
        }

        /* Parse GROUP definitions outside MAIN */
        if (!in_main && (strncmp(line, "GROUP", 5) == 0 || line[0] == '{')) {
            char group_name[256] = "";

            /* Extract group name */
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

            /* Parse group content */
            mensur *group_men = parse_group_content((const char**)all_lines, &i, line_count);

            if (group_count < MAX_GROUPS && group_men) {
                strncpy(groups[group_count].name, group_name, 255);
                groups[group_count].name[255] = '\0';
                groups[group_count].men = group_men;
                group_count++;
            }
        }

        free(line);
    }

    /* Free lines */
    for (i = 0; i < line_count; i++) {
        free(all_lines[i]);
    }
}

/*
 * Phase 3: Build main mensur structure from MAIN section
 */
static mensur* build_xmen(const char *buffer) {
    mensur *head = NULL, *cur = NULL;

    /* Split buffer into lines */
    char *buf = strdup(buffer);
    char *all_lines[10000];
    int line_count = 0;

    char *line = strtok(buf, "\n");
    while (line != NULL && line_count < 10000) {
        all_lines[line_count++] = strdup(line);
        line = strtok(NULL, "\n");
    }
    free(buf);

    /* Find and parse MAIN section */
    int in_main = 0;
    int in_nested_group = 0;

    for (int i = 0; i < line_count; i++) {
        char *line = trim_line(strdup(all_lines[i]));

        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        /* Check for MAIN start */
        if (!in_main && (strcmp(line, "MAIN") == 0 || strcmp(line, "[") == 0)) {
            in_main = 1;
            free(line);
            continue;
        }

        if (!in_main) {
            free(line);
            continue;
        }

        /* Check for MAIN end */
        if (strcmp(line, "END_MAIN") == 0 || strcmp(line, "]") == 0) {
            in_main = 0;
            free(line);
            break;
        }

        /* Handle nested GROUP markers (just skip them - inline expansion) */
        if (strncmp(line, "GROUP", 5) == 0 || line[0] == '{') {
            in_nested_group = 1;
            free(line);
            continue;
        }
        if (strcmp(line, "END_GROUP") == 0 || strcmp(line, "}") == 0) {
            in_nested_group = 0;
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

        /* Handle BRANCH/MERGE/SPLIT directives */
        if (strncmp(line, "BRANCH", 6) == 0 || strncmp(line, "MERGE", 5) == 0 ||
            strncmp(line, "SPLIT", 5) == 0 || line[0] == '>' || line[0] == '<' || line[0] == '|') {

            char directive;
            char *p = line;

            if (strncmp(p, "BRANCH", 6) == 0) {
                directive = '>';  /* BRANCH -> SPLIT */
                p += 6;
            } else if (strncmp(p, "MERGE", 5) == 0) {
                directive = '<';  /* MERGE -> JOIN */
                p += 5;
            } else if (strncmp(p, "SPLIT", 5) == 0) {
                directive = '|';
                p += 5;
            } else {
                directive = *p++;
            }

            /* Skip comma */
            p = skip_whitespace(p);
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
                char *end = name + strlen(name) - 1;
                while (end >= name && isspace((unsigned char)*end)) *end-- = '\0';
                ratio = evaluate_expression(skip_whitespace(comma + 1));
            } else {
                strncpy(name, p, 255);
                name[255] = '\0';
                char *end = name + strlen(name) - 1;
                while (end >= name && (isspace((unsigned char)*end) || *end == ',')) *end-- = '\0';
            }

            /* Create marker segment */
            if (cur) {
                mensur *marker = append_men(cur, cur->db, cur->db, 0, "");
                strncpy(marker->sidename, name, sizeof(marker->sidename) - 1);
                marker->sidename[15] = '\0';
                marker->s_ratio = ratio;

                if (directive == '>') {
                    marker->s_type = SPLIT;
                } else if (directive == '<') {
                    marker->s_type = JOIN;
                } else if (directive == '|') {
                    marker->s_type = TONEHOLE;
                }

                cur = marker;
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

    /* Free lines */
    for (int i = 0; i < line_count; i++) {
        free(all_lines[i]);
    }

    return head;
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
 * Validate XMENSUR file structure:
 * - Check for duplicate GROUP names
 * - Check for multiple MAIN definitions
 * Returns 1 if valid, 0 if invalid
 */
static int validate_xmensur_structure(const char *buffer) {
    char *buf = strdup(buffer);
    char *line = strtok(buf, "\n");

    int main_count = 0;
    char group_names[MAX_GROUPS][256];
    int group_name_count = 0;
    int in_main = 0;

    while (line != NULL) {
        char *trimmed = trim_line(strdup(line));

        if (strlen(trimmed) > 0) {
            /* Check for MAIN definition */
            if (strcmp(trimmed, "MAIN") == 0 || strcmp(trimmed, "[") == 0) {
                main_count++;
                in_main = 1;
                if (main_count > 1) {
                    fprintf(stderr, "Error: Multiple MAIN definitions found in XMENSUR file\n");
                    free(trimmed);
                    free(buf);
                    return 0;
                }
            }

            /* Check for END_MAIN */
            if (strcmp(trimmed, "END_MAIN") == 0 || strcmp(trimmed, "]") == 0) {
                in_main = 0;
            }

            /* Check for GROUP definition (only outside MAIN) */
            if (!in_main && (strncmp(trimmed, "GROUP", 5) == 0 || trimmed[0] == '{')) {
                /* Extract group name */
                char *p = trimmed;
                if (strncmp(p, "GROUP", 5) == 0) {
                    p += 5;
                    p = skip_whitespace(p);
                    if (*p == ',') p++;
                    p = skip_whitespace(p);
                } else {
                    p++; /* Skip '{' */
                    p = skip_whitespace(p);
                }

                /* Get group name */
                char name[256];
                int i = 0;
                while (*p && *p != ',' && !isspace((unsigned char)*p) && i < 255) {
                    name[i++] = *p++;
                }
                name[i] = '\0';

                if (strlen(name) > 0) {
                    /* Check for duplicate */
                    for (int j = 0; j < group_name_count; j++) {
                        if (strcmp(group_names[j], name) == 0) {
                            fprintf(stderr, "Error: Duplicate GROUP definition '%s' found in XMENSUR file\n", name);
                            free(trimmed);
                            free(buf);
                            return 0;
                        }
                    }

                    /* Add to list */
                    if (group_name_count < MAX_GROUPS) {
                        strncpy(group_names[group_name_count], name, 255);
                        group_names[group_name_count][255] = '\0';
                        group_name_count++;
                    }
                }
            }
        }

        free(trimmed);
        line = strtok(NULL, "\n");
    }

    free(buf);

    /* Check that MAIN was defined */
    if (main_count == 0) {
        fprintf(stderr, "Error: No MAIN definition found in XMENSUR file\n");
        return 0;
    }

    return 1;
}

/*
 * Main entry point: Read XMENSUR file
 */
mensur* read_xmensur(const char *path) {
    int err;
    FILE *infile;
    mensur *men = NULL;
    struct stat fstatus;
    unsigned long readbytes;
    char *readbuffer;

    /* Open and read entire file */
    if ((err = stat(path, &fstatus)) != 0) {
        fprintf(stderr, "Failed to open XMENSUR file: %s\n", path);
        return NULL;
    }

    readbytes = fstatus.st_size;
    readbuffer = malloc(readbytes + 1);
    if (readbuffer == NULL) {
        fprintf(stderr, "Cannot allocate memory\n");
        return NULL;
    }

    infile = fopen(path, "r");
    if (infile == NULL) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        free(readbuffer);
        return NULL;
    }

    fread(readbuffer, 1, readbytes, infile);
    readbuffer[readbytes] = '\0';
    fclose(infile);

    eol_to_lf(readbuffer);

    /* Phase 0: Validate structure */
    if (!validate_xmensur_structure(readbuffer)) {
        free(readbuffer);
        return NULL;
    }

    /* Phase 1: Parse variables */
    read_xmen_variables(readbuffer);

    /* Phase 2: Read child mensur groups */
    read_xmen_groups(readbuffer);

    /* Phase 3: Build main mensur structure */
    men = build_xmen(readbuffer);

    free(readbuffer);

    if (men == NULL) {
        return NULL;
    }

    /* Phase 4: Resolve child connections */
    resolve_xmen_child(men);

    /* Phase 5: Rejoint branches */
    men = rejoint_men(men);

    return get_first_men(men);
}
