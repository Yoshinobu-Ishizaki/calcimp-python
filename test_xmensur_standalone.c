#include <stdio.h>
#include <stdlib.h>
#include "src/xmensur.h"

int main() {
    printf("Testing XMENSUR parser with sample/test.xmen...\n");

    xmen_group *groups = NULL;
    int group_count = 0;

    int result = read_xmensur_file("sample/test.xmen", &groups, &group_count);

    if (result == 0) {
        printf("SUCCESS: Parsed %d groups\n", group_count);

        /* Print MAIN group details */
        for (int i = 0; i < group_count; i++) {
            printf("Group %d: name='%s', count=%d\n", i, groups[i].name, groups[i].count);
        }

        /* Free allocated memory */
        free_xmen_groups(groups, group_count);
        return 0;
    } else {
        printf("FAILED: Parser returned error\n");
        return 1;
    }
}
