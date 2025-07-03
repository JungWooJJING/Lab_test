#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdint.h>

#define TMP_IN "/tmp/.cur_input"     

void check(const char *buf) {
    printf("start!\n");
    if (buf[0] == 'I') {
        printf("Correct I!\n");
        if (buf[1] == 't') {
            printf("Correct t!\n");
            if (buf[2] == 'e') {
                printf("Correct e!\n");
                if (buf[3] == 's') {
                    printf("Correct s!\n");
                    if (buf[4] == 't') {
                        printf("Correct t!\n");
                        if (buf[5] == 'f') {
                            printf("Correct f!\n");
                            if (buf[6] == 'o') {
                                printf("Correct o!\n");
                                if (buf[7] == 'r') {
                                    printf("Correct r!\n");
                                    if (buf[8] == 'k') {
                                        printf("Correct k!\n");
                                        if (buf[9] == 's') {
                                            printf("Correct s!\n");
                                            if (buf[10] == 'e') {
                                                printf("Correct e!\n");
                                                if (buf[11] == 'r') {
                                                    printf("Correct r!\n");
                                                    if (buf[12] == 'v') {
                                                        printf("Correct v!\n");
                                                        if (buf[13] == 'e') {
                                                            printf("Correct e!\n");
                                                            if (buf[14] == 'r') {
                                                                printf("Correct all!\n");
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


int main() {

    FILE *fp = fopen(TMP_IN, "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    char buf[256];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);

    check(buf);
    return 0;
}
