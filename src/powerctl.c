/*=====================================================================
 *  powerctl.c - Reboot / Shutdown helper program
 *
 *  Should be invoked using the poweroff / reboot scripts, in persistent mode.
 *
 *  The program can be invoked as:
 *      progname --reboot    -> sync files, try to clean up /root,
 *                              then issue a kernel reboot.
 *      progname --shutdown  -> sync files, try to clean up /root,
 *                              then power‑off the machine.
 *
 *  It attempts to kill all remaining 'bash' processes (and any other
 *  processes that share the same executable name) before unmounting
 *  the '/root' filesystem.
 *  Init performs the actual shutdown or reboot action.
 *=====================================================================*/
#include <stdio.h>          // fopen, fclose, perror, fprintf, etc.
#include <stdlib.h>         // malloc, free, exit, etc.
#include <string.h>         // strcmp, strdup, strcspn, etc.
#include <unistd.h>         // fork, execlp, sync, etc.
#include <mntent.h>         // setmntent, getmntent, endmntent (mount table)
#include <sys/mount.h>      // umount2, MNT_FORCE
#include <fcntl.h>          // open flags (O_WRONLY, O_CREAT)
#include <sys/wait.h>       // waitpid
#include <sys/types.h>      // pid_t, etc.
#include <dirent.h>         // opendir, readdir, closedir
#include <ctype.h>          // isdigit

/*--------------------------------------------------------------------
 *  Constants
 *--------------------------------------------------------------------*/
#define MAX_PROCS 4096
#define MAX_NAME_LEN 256

/*=====================================================================
 *  get_comms()
 *
 *  Scan /proc for numeric directories (each representing a running
 *  process), read the /proc/<pid>/comm file (which contains the name
 *  of the executable), and build a **unique** list of those names.
 *
 *  Parameters
 *      out_count  - pointer where the function stores how many unique
 *                   names were found.
 *
 *  Returns
 *      Dynamically allocated array of C strings (char **).  The caller
 *      must free each string (using free()) and then free the array
 *      itself.  Returns NULL on allocation or directory‑open error.
 *=====================================================================*/
char **get_comms(int *out_count) {
    DIR *dir;                                           // directory stream for /proc
    struct dirent *entry;                               // each entry inside /proc
    char path[512];                                     // buffer for "/proc/<pid>/comm"
    char comm[MAX_NAME_LEN];                            // buffer for a single comm string
    char **names = malloc(sizeof(char *) * MAX_PROCS);  // result array
    int count = 0;                                      // number of unique names collected

    
    if (!names) return NULL; // allocation failure -> give up

    dir = opendir("/proc"); // open the proc filesystem
    if (!dir) {
        perror("opendir");
        free(names);
        return NULL;
    }

    /*----------------------------------------------------------------
     *  Walk through every entry in /proc
     *----------------------------------------------------------------*/
    while ((entry = readdir(dir)) != NULL) {
        // Skip non‑numeric entries - they are not PIDs
        if (!isdigit(entry->d_name[0]))
            continue;

        // Build the path to the comm file for this PID
        snprintf(path, sizeof(path) - 1, "/proc/%s/comm", entry->d_name);
        path[sizeof(path) - 1] = '\0'; // safety null‑termination

        FILE *f = fopen(path, "r");
        if (!f)                        // process may have vanished; ignore it
            continue;

        // Read the first line (the command name)
        if (fgets(comm, sizeof(comm), f)) {
            // Strip trailing newline, if any
            comm[strcspn(comm, "\n")] = '\0';

            /*--------------------------------------------------------
             *  Check whether this name is already in the list.
             *  Deduplication because killall, kills all processes.
             *--------------------------------------------------------*/
            int duplicate = 0;
            for (int i = 0; i < count; i++) {
                if (strcmp(names[i], comm) == 0) {
                    duplicate = 1;
                    break;
                }
            }
            
            // If it is new, store a copy of the string
            if (!duplicate) {
                names[count] = strdup(comm);
                if (++count >= MAX_PROCS) // safety guard - stop if limit reached
                    break;
            }
        }
        fclose(f);
    }
    closedir(dir);
    *out_count = count; // tell caller how many entries we have 
    return names;
}

/*=====================================================================
 *  deinit()
 *
 *  1. Verify that /root is currently a mounted filesystem.
 *  2. If it is, create a temporary marker file (/tmp/poweroff) - this
 *     is used by init to detect that a power‑off sequence is in progress.
 *  3. Fork a child that:
 *        a) Retrieves the list of unique process names via get_comms().
 *        b) For each process, run `killall -9 <name>`
 *           Do this for every process except bash.
 *           Kill bash as last entry, because it'll trigger code in init.
 *  4. Parent waits for the child to finish.
 *  5. Attempt to unmount /root with MNT_FORCE.
 *
 *  Returns
 *      0  - success (either /root was not mounted or it was unmounted)
 *     -1  - error while checking mounts or unmounting
 *      1  - error while handling temporary file or fork/exec
 *=====================================================================*/
int deinit(const char *paction) {
    FILE *mntfile;        // file handle for /proc/mounts
    struct mntent *ent;   // each mount entry
    int is_mounted = 0;   // flag: is /root currently mounted?

    /*------------------------------------------------------------
     *  Open the mount table and look for "/root"
     *------------------------------------------------------------*/
    mntfile = setmntent("/proc/mounts", "r");
    if (!mntfile) {
        perror("setmntent");
        return -1;
    }

    while ((ent = getmntent(mntfile)) != NULL) {
        if (strcmp(ent->mnt_dir, "/root") == 0) {
            is_mounted = 1;
            break;
        }
    }

    endmntent(mntfile);
    
    /*------------------------------------------------------------
     *  Create a marker file so that the init program can
     *  detect that a power‑off is underway.
     *------------------------------------------------------------*/
    int fd = open("/tmp/poweroff", O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    
    if (write(fd, paction, strlen(paction)) == -1) {
        perror("write");
        close(fd);
        return 1;
    }
    
    close(fd);

    /*------------------------------------------------------------
     *  Fork a child that will kill remaining processes.
     *------------------------------------------------------------*/
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // child process
        int count = 0;
        char **comms = get_comms(&count);
        if (!comms) {
            fprintf(stderr, "Failed to get comms\n");
            return 1;
        }

        // Iterate over the unique program names
        for (int i = 0; i < count; i++) {
            // If the name is not "bash" or "init", kill all instances of the program
            if (!(strcmp(comms[i], "bash"))) {
                if (!(strcmp(comms[i], "init"))) {
                    execlp("killall", "killall", "-9", comms[i], (char *)NULL);
                    perror("execlp");
                }
            }
            free(comms[i]);
        }
        free(comms);

        // Kill all bash processes lastly
        execlp("killall", "killall", "-9", "bash", (char *)NULL);
        perror("execlp");
        exit(1);
    } else {
        // parent process
        int status;
        waitpid(pid, &status, 0); // wait for the child to finish
    }

    /*------------------------------------------------------------
     *  Finally, force‑unmount /root.  MNT_FORCE allows us to detach
     *  even if the filesystem is busy.
     *------------------------------------------------------------*/
    if (is_mounted) {
        if (umount2("/root", MNT_FORCE) == 0) {
            return 0; // success
        } else {
            perror("umount");
            return -1; // unmount failed
        }
    }
}

/*=====================================================================
 *  main()
 *
 *  Very small command‑line driver:
 *      - Verify exactly one argument is supplied.
 *      - Call sync() to flush buffered data to disk.
 *      - Run deinit() to clean up /root and cause init to reboot or shutdown.
 *
 *  Returns 0 on success, 1 on usage errors.
 *=====================================================================*/
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [--reboot | --shutdown]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--reboot") == 0) {
        sync();   // ensure all pending writes are flushed
        deinit("reboot"); // cleans up before rebooting, actual reboot or shutdown happens inside init
    } else if (strcmp(argv[1], "--shutdown") == 0) {
        sync();
        deinit("shutdown");
    } else {
        // Unrecognised option - print help and exit with error
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        fprintf(stderr, "Usage: %s [--reboot | --shutdown]\n", argv[0]);
        return 1;
    }

    return 0;
}
