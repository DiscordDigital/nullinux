#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/reboot.h>     // reboot(), LINUX_REBOOT_* constants
#include <linux/reboot.h>   // LINUX_REBOOT_CMD_* values

// The path to the bash binary
const char BASH_PATH[] = "/usr/bin/bash";

/* Runs a script synchronously and returns the error code
 *  - file : The file to run using bash
 *
 * In a sense "file" can be a command too, as it just runs bash with "-c" parameter.
 */
int run(char *file) {
    const char *args[] = {BASH_PATH, "-noprofile", "-norc", "-c", file, NULL};
    pid_t pid = fork();

    // fork error
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // child process
        execv(BASH_PATH, (char * const *)args);
        perror("execv");
        _exit(127);
    }

    // parent process
    int status;
    (void)waitpid(pid, &status, 0);

    // Returns true if the child exited normally
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    // Returns true if the child was terminated by a signal
    if (WIFSIGNALED(status)) {
        // Starting from 128 because the other error codes are reserved
        return 128 + WTERMSIG(status);
    }

    // Unexpected case
    return -2;
}

/* Helper that wraps the mount system call.
 *  - source : what to mount (e.g. "proc")
 *  - target : where to mount it (e.g. "/proc")
 *  - type   : filesystem type (e.g. "proc")
 *
 * On failure we print the target name (via perror) and abort the program.
 */
void mount_fs(const char *source, const char *target, const char *type) {
    if (mount(source, target, type, 0, "") != 0) {
        perror(target);
        exit(1);
    }
}

/* ----------------------------------------------------------------------
 *  main - set up a minimal chroot‑like environment and drop into a bash
 * ---------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    // Mount the essential pseudo‑filesystems that a normal Linux userspace expects to find.
    mount_fs("devtmpfs", "/dev", "devtmpfs");
    mount_fs("proc", "/proc", "proc");
    mount_fs("sysfs", "/sys", "sysfs");

    /* Create the standard file‑descriptor symlinks that many programs use.
     * The leading "(void)! ..." silences the compiler warning about the
     * return value while still executing the call. */
    (void)!symlink("/proc/self/fd", "/dev/fd");         // generic FD directory
    (void)!symlink("/proc/self/fd/0", "/dev/stdin");    // stdin
    (void)!symlink("/proc/self/fd/1", "/dev/stdout");   // stdout
    (void)!symlink("/proc/self/fd/2", "/dev/stderr");   // stderr

    // Set up a pseudo‑terminal master directory - required for ssh sessions.
    mkdir("/dev/pts", 0755);                            // create mount point
    mount_fs("devpts", "/dev/pts", "devpts");           // mount devpts

    // Become a session leader so that the shell we later start has its own controlling terminal.
    if (setsid() < 0) {
        perror("setsid");
        exit(1);
    }

    /* Explicitly assign the controlling terminal (tty) to file descriptor 0.
     * TIOCSCTTY makes the file descriptor become the controlling tty. */
    ioctl(0, TIOCSCTTY, 1);

    // Set a few environment variables that many shells expect.
    setenv("HOME", "/root", 1);
    setenv("USER", "root", 1);
    setenv("TERM", "linux", 1);

    // Path to the shell we will exec and its argument vector.
    const char *args[] = {BASH_PATH, NULL};

    /* Install a SIGCHLD handler that does nothing, but with SA_NOCLDWAIT set 
     * This prevents zombie processes being created in /proc */
    struct sigaction sa = {0};
    sa.sa_handler = SIG_DFL;          // default handler - ignored
    sa.sa_flags   = SA_NOCLDWAIT;     // do not create zombies
    sigaction(SIGCHLD, &sa, NULL);

    // Predefine variable paction for poweroff or reboot action, when init ends.
    char paction[16];

    // Run synchronous boot tasks
    (void)run("/opt/init.0/boot.sh");

    /* Main loop - keep respawning a bash after it exits.
     * This mimics an init‑style process that stays alive forever. */
    while (1) {
        pid_t pid = fork();                           // create child
        if (pid == 0) {                               // child process
            (void)!chdir("/root");                    // cd to /root
            execv(BASH_PATH, (char * const *)args);   // execute bash
            perror("execv");                          // execv only returns on error
        }
        
        // parent: wait for the child (the shell) to terminate
        waitpid(pid, NULL, 0);
        
        /* 9. After a shell exit, check for a special "poweroff" flag.
         *    If the file /tmp/poweroff exists perform action, then spin forever
         *    this mimics a halted system where nothing else should run. */
        if (access("/tmp/poweroff", F_OK) != -1) {
            int fd = open("/tmp/poweroff", O_RDONLY);
            if (fd == -1) {
                perror("open");
                return 1;
            }
            
            ssize_t n = read(fd, paction, sizeof(paction) - 1);
            
            if (n == -1) {
                perror("read");
                close(fd);
                return 1;
            }

            paction[n] = '\0';
            
            if (strcmp(paction, "reboot") == 0) {
                reboot(LINUX_REBOOT_CMD_RESTART);
            } else if (strcmp(paction, "shutdown") == 0) {
                reboot(LINUX_REBOOT_CMD_POWER_OFF);
            } else {
                reboot(LINUX_REBOOT_CMD_POWER_OFF);
            }
            
            pause(); // sleep until a signal arrives - act as a halted state
        }
        
        // otherwise the loop repeats and a new bash is spawned 
    }
}
