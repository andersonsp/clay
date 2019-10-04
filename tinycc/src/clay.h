#ifndef _CLAY_H
#define _CLAY_H

void clay_watch_start(const char* filename); // relative to "data" directory
void clay_watch_stop(void);
int clay_watch_changed(void);

void clay_plugin_load(const char* filename); // relative to "plugin" directory
void clay_plugin_close(void);
int clay_plugin_step(void);

typedef enum {
    CP_LOAD = 0,
    CP_STEP = 1,
    CP_UNLOAD = 2,
    CP_CLOSE = 3,
} ClayOp;

int clay_plugin_main(ClayOp op);

#endif // _CLAY_H
