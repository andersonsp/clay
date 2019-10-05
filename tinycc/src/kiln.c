
#include "tcc.h"
#include "clay.h"
#include <dirent.h>
#include "tcctools.c"

static const char help[] =
    "Clay "TCC_VERSION"\n"
    "Usage: clay -lSDL\n"
    "       clay -lglfw\n"
    "       clay [options...] infile -- [arguments...]\n"
    "General options:\n"
    "  -v -vv      show version, show search paths or loaded files\n"
    "  -h -hh      show this, show more help\n"
    "  -bench      show statistics\n"
    "  -llib       load dynamic library 'lib'\n"
#ifdef CONFIG_TCC_BCHECK
    "  -b          disable built-in memory and bounds checker\n"
#endif
#ifdef CONFIG_TCC_BACKTRACE
    "  -bt N       show N callers in stack traces\n"
#endif
;

static const char help2[] = "Clay "TCC_VERSION" - More Options\n";

static const char version[] =
    "clay version "TCC_VERSION" ("
        "x86_64"
#ifdef TCC_TARGET_PE
        " Windows"
#elif defined(TCC_TARGET_MACHO)
        " Darwin"
#else
        " Linux"
#endif
    ")\n"
    ;

static void print_dirs(const char *msg, char **paths, int nb_paths) {
    printf("%s:\n%s", msg, nb_paths ? "" : "  -\n");
    for(int i = 0; i < nb_paths; i++) printf("  %s\n", paths[i]);
}

static void print_search_dirs(TCCState *s) {
    printf("install: %s\n", s->tcc_lib_path);
    /* print_dirs("programs", NULL, 0); */
    print_dirs("include", s->sysinclude_paths, s->nb_sysinclude_paths);
    print_dirs("libraries", s->library_paths, s->nb_library_paths);
    printf("libtcc1:\n  %s/"TCC_LIBTCC1"\n", s->tcc_lib_path);
#ifndef TCC_TARGET_PE
    print_dirs("crt", s->crt_paths, s->nb_crt_paths);
    printf("elfinterp:\n  %s\n",  DEFAULT_ELFINTERP(s));
#endif
}

static unsigned getclock_ms(void) {
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000 + (tv.tv_usec+500)/1000;
#endif
}

typedef int (*PluginMain)(int);
typedef struct ClayFwatch ClayFwatch;
struct Clay {
    TCCState *main;
    char path[4096];

    // plugin handling stuff
    char plugin_file[1024];
    char plugin_path[1024];
    int plugin_path_sz;

    TCCState *plugin, *pback;
    ClayFwatch *plugin_dep[16];
    int plugin_nb_dep;

    PluginMain plugin_main;

    // data watcher stuff
    #ifdef __APPLE__
    int kq; // global kqueue
    #endif

    ClayFwatch *watcher;
} g_clay;


#if defined(__APPLE__)
    #include <libproc.h>
    #include <sys/event.h>
    #include <err.h>

    #define DLLSUF ".dylib"

    struct ClayFwatch {
        struct kevent mevent; // monitored event */
        int fd, ret;
        char filename[1024];
    };

    static void clay_set_path(void) {
        if (proc_pidpath(getpid(), g_clay.path, 4096) <= 0) {
            fprintf(stderr, "pid_path(): %s\n", strerror(errno));
            exit(1);
        }
        char *basedir = strrchr(g_clay.path, '/');
        if (basedir) *basedir = '\0';
    }

    static ClayFwatch* clay_fwatch_new(const char* filename) {
        if (g_clay.kq == -1) { // kqueue uninitialized
            g_clay.kq = kqueue();
            if (g_clay.kq  == -1) err(EXIT_FAILURE, "kqueue() failed");
        }

        int fd = open(filename, O_RDONLY);
        if (fd  == -1) err(EXIT_FAILURE, "Failed to open '%s'", filename);

        ClayFwatch* fw = calloc(sizeof(ClayFwatch), 1);
        EV_SET(&fw->mevent, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME, 0, NULL);

        int ret = kevent(g_clay.kq, &fw->mevent, 1, NULL, 0, NULL);
        if (ret == -1) err(EXIT_FAILURE, "kevent register");
        if (fw->mevent.flags & EV_ERROR) errx(EXIT_FAILURE,  "Event error: %s", strerror((int) fw->mevent.data));
        fw->fd = fd;

        strcpy(fw->filename, filename);
        return fw;
    }

    static int clay_fwatch_changed(ClayFwatch* fw) {
        struct timespec ts = {0};
        struct kevent tevent = {0}; // triggered event
        int ret = kevent(g_clay.kq, NULL, 0, &tevent,  1, &ts); // don't wait
        if  (ret == -1) err(EXIT_FAILURE, "kevent error");

        if (ret > 0) {
            // char *event_name;
            // if (NOTE_DELETE & tevent.fflags) event_name="DELETE";
            // else if (NOTE_WRITE & tevent.fflags) event_name="WRITE";
            // else if (NOTE_RENAME & tevent.fflags) event_name="RENAME";
            // else event_name="UNKNOWN_EVENT";
            // printf("%s %s (%i)\n", event_name, fw->filename, tevent.fflags);
            return 1;
        }
        return 0;
    }

    static void clay_fwatch_destroy(ClayFwatch* fw) {
        close(g_clay.watcher->fd);
        tcc_free(g_clay.watcher);
    }

#elif defined(__linux__)
    #define DLLSUF ".so"

    struct ClayFwatch {
        char filename[1024];
    };

    static void clay_set_path(void) {
        readlink( "/proc/self/exe", g_clay.path, 4096);
        char *basedir = strrchr(g_clay.path, '/');
        if (basedir) *basedir = '\0';
    }

    static ClayFwatch* clay_fwatch_new(const char* filename) {

        ClayFwatch* fw = calloc(sizeof(ClayFwatch), 1);

        strcpy(fw->filename, filename);
        return fw;
    }

    static int clay_fwatch_changed(ClayFwatch* fw) {

        return 0;
    }

    static void clay_fwatch_destroy(ClayFwatch* fw) {
    }

#elif defined(_WIN32)
    #define DLLSUF ".dll"

    struct ClayFwatch {
        char filename[1024];
        HANDLE directory;

        OVERLAPPED o;
        DWORD b;

        union {
            FILE_NOTIFY_INFORMATION i;
            char d[sizeof(FILE_NOTIFY_INFORMATION)+MAX_PATH];
        } fni;
    };

    static void clay_set_path(void) {
        HINSTANCE hinst = GetModuleHandle(NULL);

        GetModuleFileNameA(hinst, g_clay.path, 4096);
        normalize_slashes(strlwr(g_clay.path));
        char *basedir = strrchr(g_clay.path, '/');
        if (basedir) *basedir = '\0';
    }

    static ClayFwatch* clay_fwatch_new(const char* filename) {
        ClayFwatch* fw = calloc(sizeof(ClayFwatch), 1);

        // CP_UTF8,            // UINT         CodePage,
        //     MB_PRECOMPOSED,     // DWORD        dwFlags,
        //     path,               // _In_NLS_string_(cbMultiByte)LPCCH lpMultiByteStr,
        //     -1,                 // strlen(path),    // int  cbMultiByte,
        //     wpath,              // LPWSTR lpWideCharStr,
        //     1024                // int cchWideChar
        // WCHAR wpath[1024];
        // MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, g_clay.path, -1, wpath, 1024);

        // BOOL success = ReadDirectoryChangesW(directory,
        //                                     watcher->read_buffer,
        //                                     sizeof( watcher->read_buffer ),
        //                                     FALSE, // watcher->recursive
        //                                     FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME,
        //                                     0x0,
        //                                     &watcher->overlapped,
        //                                     0x0);

        // if(!success) tcc_error("could not set the watch\n");


        fw->directory = CreateFile(g_clay.path,
            FILE_LIST_DIRECTORY | GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            0
            );

        fw->o.hEvent = CreateEvent(0,0,0,0);
        BOOL success = ReadDirectoryChangesW(fw->directory, &fw->fni, sizeof(fw->fni), FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE, &fw->b, &fw->o, 0);

        if(!success) tcc_error("could not set the watch\n");
        strcpy(fw->filename, filename);
        return fw;
    }

    static int clay_fwatch_changed(ClayFwatch* fw) {
        GetOverlappedResult(fw->directory, &fw->o, &fw->b, FALSE);
        if (fw->fni.i.Action != 0) {
            // wprintf(L"action %d, b: %d, %s\n", fni.i.Action, b, fni.i.FileName);
            fw->fni.i.Action = 0;

            // set the watch again
            ReadDirectoryChangesW(fw->directory, &fw->fni, sizeof(fw->fni), FALSE,
                FILE_NOTIFY_CHANGE_LAST_WRITE, &fw->b, &fw->o, 0);

            return 1;
        }
        return 0;
    }

    static void clay_fwatch_destroy(ClayFwatch* fw) {
    }
#else
    #error "Unsupported platform"
#endif

static void clay_init_tcc(TCCState *s) {
    tcc_set_lib_path(s, g_clay.path);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    char path[512];
    sprintf(path, "%s/include/libc", s->tcc_lib_path);
    tcc_add_include_path(s, path);

    sprintf(path, "%s/lib", s->tcc_lib_path);
    tcc_add_include_path(s, path);
    tcc_define_symbol(s, "__CLAY__", NULL);

    #ifdef __APPLE__
    tcc_define_symbol(s, "__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__", "1060");
    tcc_define_symbol(s, "TARGET_CPU_X86_64", "1");
    tcc_define_symbol(s, "TARGET_OS_MAC", "1");
    #endif
}

static int find_section_idx(const char *name) {
    for(int i = 1; i < g_clay.main->nb_sections; i++) {
        if(strcmp(g_clay.main->sections[i]->name, name) == 0) return i;
    }
    tcc_error("Section not found in Main program\n"); // should never happen
    return 0;
}

static void clay_plugin_init(TCCState *s) {
    int text_idx = find_section_idx(".text");
    int data_idx = find_section_idx(".data");

    Section *sec = g_clay.main->symtab;
    ElfW(Sym) *sym;
    for (sym = (ElfW(Sym) *) sec->data; sym < (ElfW(Sym) *)(sec->data+sec->data_offset); sym++) {
        if (sym->st_shndx != text_idx && sym->st_shndx != data_idx) continue;
        // if (sym->st_shndx == SHN_UNDEF) continue;

        int info = ELFW(ST_TYPE)(sym->st_info);
        if (ELFW(ST_BIND)(sym->st_info) == STB_GLOBAL && (info == STT_FUNC || info == STT_OBJECT)) {
            char *name = (char *) sec->link->data + sym->st_name;
            tcc_add_symbol(s, name, tcc_get_symbol(g_clay.main, name));
            // printf("sym %d: %s*\n", sym->st_shndx, name);
        }
    }

    if (tcc_add_file(s, g_clay.plugin_file) == -1) tcc_error("failed to compile plugin\n");;
    if (tcc_relocate(s, TCC_RELOCATE_AUTO) < 0) tcc_error("failed to load plugin\n");

    g_clay.plugin_main = (int (*)(int)) tcc_get_symbol(s, "clay_plugin_main");
    if (!g_clay.plugin_main) tcc_error("plugin main() function not found\n");

    g_clay.plugin = s;
    g_clay.plugin_main(CP_LOAD);
}

// int check_dependencies(ClayPlugin* cp) {
//     int deps = 0;
//     for (int i=0; i<cp->s->nb_target_deps; ++i) {
//         if (strncmp(path, cp->s->target_deps[i], sz) == 0) {
//             if (deps < cp->nb_dep) {
//                 strcmp(cp->dep[deps++], cp->s->target_deps[i]);
//                 fprintf(stdout, " %s*\n", cp->s->target_deps[i]);
//             } else {
//                 return 1;
//             }
//         }
//     }
//     return 0;
// }

void clay_plugin_load(const char* filename) {
    TCCState *s = tcc_new();
    clay_init_tcc(s);
    s->do_bounds_check = 1; // enable bounds check for plugins
    s->do_debug = 1;

    sprintf(g_clay.plugin_file, "%s/%s", g_clay.path, filename);
    strcpy(g_clay.plugin_path, g_clay.plugin_file);

    #ifdef _WIN32
    normalize_slashes(g_clay.plugin_path);
    #endif
    char *basepath = strrchr(g_clay.plugin_path, '/');
    if (basepath) *basepath = '\0';
    else tcc_error("cant compute plugin path\n");

    g_clay.plugin_path_sz = strlen(g_clay.plugin_path);

    // print_search_dirs(s);
    clay_plugin_init(s);

    printf("deps:\n");
    for (int i=0; i<g_clay.plugin->nb_target_deps; ++i) {
        if (strncmp(g_clay.plugin_path, g_clay.plugin->target_deps[i], g_clay.plugin_path_sz) == 0) {
            if (g_clay.plugin_nb_dep < 16) {
                g_clay.plugin_dep[g_clay.plugin_nb_dep++] = clay_fwatch_new(g_clay.plugin->target_deps[i]);
                fprintf(stdout, " %s*\n", g_clay.plugin->target_deps[i]);
            } else {
                fprintf(stdout, " %s\n", g_clay.plugin->target_deps[i]);
            }
        }
    }
}

int clay_plugin_step(void) {
    if (!g_clay.plugin) return 0;

    int changed = 0;
    for (int i=0; i < g_clay.plugin_nb_dep; i++) {
        changed += clay_fwatch_changed(g_clay.plugin_dep[i]);
    }
    if (changed > 0) {
        g_clay.plugin_main(CP_UNLOAD);
        TCCState *s = tcc_new();
        clay_init_tcc(s);
        clay_plugin_init(s);

        if (g_clay.pback) tcc_delete(g_clay.pback);
        g_clay.pback = g_clay.plugin;
        g_clay.plugin = s;
    }
    g_clay.plugin_main(CP_STEP);
    return 0;
}

void clay_plugin_close(void) {
    if (!g_clay.plugin) return;

    g_clay.plugin_main(CP_CLOSE);
    if (g_clay.pback) tcc_delete(g_clay.pback);
    tcc_delete(g_clay.plugin);

    g_clay.pback = NULL;
    g_clay.plugin = NULL;
}

void clay_watch_start(const char* filename) {
    if (g_clay.watcher) clay_watch_stop();
    char path[1024];
    sprintf(path, "%s/%s", g_clay.path, filename);
    g_clay.watcher = clay_fwatch_new(path);
}

void clay_watch_stop(void) {
    if (!g_clay.watcher) return;

    clay_fwatch_destroy(g_clay.watcher);
    g_clay.watcher = NULL;
}

int clay_watch_changed() {
    if (!g_clay.watcher) return 0;

    return clay_fwatch_changed(g_clay.watcher);
}

static int clay_is_dir(const char* filename) {
    DIR *pdir = opendir(filename);
    if (!pdir) return 0;
    closedir(pdir);
    return 1;
}

static void args_parser_add_file(TCCState *s, const char* filename, int filetype) {
    struct filespec *f = tcc_malloc(sizeof *f + strlen(filename));
    f->type = filetype;
    f->alacarte = s->alacarte_link;
    strcpy(f->name, filename);
    dynarray_add(&s->files, &s->nb_files, f);
}

int main(int argc0, char **argv0) {
    int ret, opt, n = 0, t = 0;
    unsigned start_time = 0;
    char path[512];

    clay_set_path();
    FILE *ppfp = stdout;

    int argc = argc0;
    char **argv = argv0;
    TCCState *s = tcc_new();
    clay_init_tcc(s);
    g_clay.main = s;
    #ifdef __APPLE__
    g_clay.kq = -1;
    #endif

    opt = tcc_parse_args(s, &argc, &argv, 1);

    if ((n | t) == 0) {
        if (argc > 1 && opt == OPT_HELP) return printf(help), 1;
        if (opt == OPT_HELP2) return printf(help2), 1;

        if (s->verbose) printf(version);
        if (opt == OPT_AR) return tcc_tool_ar(s, argc, argv);
    #ifdef TCC_TARGET_PE
        if (opt == OPT_IMPDEF) return tcc_tool_impdef(s, argc, argv);
    #endif
        // if (opt == OPT_V) return 0;
        if (opt == OPT_PRINT_DIRS) {
            print_search_dirs(s);
            return 0;
        }

        n = s->nb_files;
        if (n-s->nb_libraries == 0) { // TODO: add a cli override for this
            sprintf(path, "%s/src", g_clay.path);
            DIR *pdir = opendir(path);
            if (pdir) {
                struct dirent *pent = NULL;
                while ((pent = readdir(pdir))) {
                    if (pent->d_name[0] == '.') continue;
                    sprintf(path, "%s/src/%s", s->tcc_lib_path, pent->d_name);
                    const char *ext = tcc_fileextension(path);
                    if (strcmp(ext, ".c") == 0 && !clay_is_dir(path)) {
                        args_parser_add_file(s, path, 0);
                    }
                }
                closedir(pdir);
            } else {
                printf("no 'src' dir found\n");
            }
        }

        if (s->nb_libraries == 0) {
            // FIX: error out
            sprintf(path, "%s/lib", g_clay.path);
            DIR *pdir = opendir(path);
            if (pdir) {
                struct dirent *pent = NULL;
                while ((pent = readdir(pdir))) {
                    if (pent->d_name[0] == '.') continue;
                    strcpy(path, pent->d_name);
                    if (strncmp("lib", path, 3) == 0 && !clay_is_dir(path)) {
                        char *basename = strrchr(path,'.');
                        if (basename) *basename = '\0';
                        args_parser_add_file(s, &path[3], AFF_TYPE_LIB);
                        s->nb_libraries++;
                    }
                }
                closedir(pdir);
            }
        }
        n = s->nb_files;
        if (n == 0)  tcc_error("no input files\n");

        if (s->option_pthread) tcc_set_options(s, "-lpthread");
        if (s->do_bench) start_time = getclock_ms();
    }

    s->ppfp = ppfp;
    if ((s->output_type == TCC_OUTPUT_MEMORY || s->output_type == TCC_OUTPUT_PREPROCESS) && (s->dflag & 16)) {
        s->dflag |= t ? 32 : 0, s->run_test = ++t, n = s->nb_files;
    }

    // compile or add each files or library
    const char *first_file;
    for (first_file = NULL, ret = 0;;) {
        struct filespec *f = s->files[s->nb_files - n];
        s->filetype = f->type;
        s->alacarte_link = f->alacarte;

        if (1 == s->verbose) printf("=> %s\n", f->name);
        if (f->type == AFF_TYPE_LIB) {
            if (tcc_add_library_err(s, f->name) < 0) ret = 1;
        } else {
            if (!first_file) first_file = f->name;
            if (tcc_add_file(s, f->name) < 0) ret = 1;
        }
        s->filetype = 0;
        s->alacarte_link = 1;

        if (--n == 0 || ret || (s->output_type == TCC_OUTPUT_OBJ && !s->option_r)) {
            break;
        }
    }

    if (s->run_test) {
        t = 0;
    } else if (s->output_type == TCC_OUTPUT_PREPROCESS) {
        ;
    } else if (0 == ret) {
        if (s->output_type == TCC_OUTPUT_MEMORY) {
            ret = tcc_run(s, argc, argv);
        } else {
            tcc_error("Can't generate output files, use TCC instead\n");
            if (s->gen_deps)
                gen_makedeps(s, s->outfile, s->deps_outfile);
        }
    }

    if (s->do_bench && (n | t | ret) == 0) tcc_print_stats(s, getclock_ms() - start_time);
    tcc_delete(s);

    if (ret == 0 && n) tcc_error("ret 0: nope\n");
    if (t) tcc_error("t: nope\n"); /* run more tests with -dt -run */

    if (ppfp && ppfp != stdout) fclose(ppfp);
    return ret;
}
