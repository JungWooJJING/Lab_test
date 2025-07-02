#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#define DEBUG_MODE 0
// #define DEBUG_MODE 1 
#define TMP_IN "/tmp/.cur_input"

uint64_t exec_cnt = 0;

int   ctl_fd[2];
int   st_fd[2];
pid_t forkserver_pid;

void copy_file(const char *src) {
    int in  = open(src, O_RDONLY);
    if (in < 0) {
        perror("open(src)");
        exit(1);
    }

    int out = open(TMP_IN, O_WRONLY | O_TRUNC | O_CREAT, 0600);
    if (out < 0) {
        perror("open(tmp)");
        exit(1);
    }

    char buf[8192];
    ssize_t n;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, n) != n) {
            perror("write(tmp)");
            exit(1);
        }
    }

    close(in);
    close(out);
}

void init_forkserver(const char *target) {
    uint32_t test = 0x74657374;     

    if (pipe(ctl_fd) == -1 || pipe(st_fd) == -1) {
        perror("pipe");
        exit(1);
    }

    forkserver_pid = fork();

    if (forkserver_pid < 0) {
        perror("fork");
        exit(1);
    }

    if (forkserver_pid == 0) {                
        dup2(ctl_fd[0], 198);
        dup2(st_fd[1], 199);
        close(ctl_fd[1]);
        close(st_fd[0]);

        if (DEBUG_MODE) {
            int devnull = open("/dev/null", O_WRONLY);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        execl(target, target, NULL);
        exit(1);                             
    }

    close(ctl_fd[0]);
    close(st_fd[1]);

    uint32_t hello = 0;
    if (read(st_fd[0], &hello, 4) != 4) {
        perror("read(hello)");
        exit(1);
    }
    if (hello != test) {
        fprintf(stderr, "forkserver hello failed (0x%x)\n", hello);
        exit(1);
    }
}

int run_target(void) {
    int req = 1;
    if (write(ctl_fd[1], &req, 4) != 4) {
        perror("write(req)");
        exit(1);
    }

    pid_t child  = 0;
    int   status = 0;

    if (read(st_fd[0], &child, 4) != 4) {
        perror("read(child)");
        exit(1);
    }
    if (read(st_fd[0], &status, 4) != 4) {
        perror("read(status)");
        exit(1);
    }

    exec_cnt++;
    return WEXITSTATUS(status);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <target_bin> <input_dir>\n", argv[0]);
        return 1;
    }

    const char *target = argv[1];
    const char *in_dir = argv[2];

    init_forkserver(target);

    DIR *dp = opendir(in_dir);
    if (!dp) {
        perror("opendir");
        return 1;
    }

    struct dirent *de;
    char path[4096];

    while ((de = readdir(dp))) {
        snprintf(path, sizeof(path), "%s/%s", in_dir, de->d_name);

        struct stat st;
        if (stat(path, &st) == -1) {
            continue;
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        copy_file(path);

        int ret = run_target();
        printf("exec_cnt: %lu, file: %s, ret: %d\n", exec_cnt, path, ret);
    }

    closedir(dp);

    return 0;
}

