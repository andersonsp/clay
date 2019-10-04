#include <clay.h>
#include <stdio.h>

void lw_plugin_loaded();
extern int one_global;

int clay_plugin_main(ClayOp op) {
    switch (op) {
        case CP_LOAD:
            lw_plugin_loaded();
            printf("plugin loaded\n");
            one_global = 52;
            return 0;
        case CP_UNLOAD:
            // if needed, save stuff to pass over to next instance
            printf("plugin unload\n");
            return 0;
        case CP_CLOSE:
            printf("plugin closed?\n");
            return 0;
        case CP_STEP:
            return 0;
    }
    return 0;
}
