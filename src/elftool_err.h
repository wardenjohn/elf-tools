#ifndef _ELFTOOL_ERR_H_
#define _ELFTOOL_ERR_H_

#include <err.h>

enum exit_status {
        EXIT_STATUS_SUCCESS             = 0,
        EXIT_STATUS_ERROR               = 1,
        EXIT_STATUS_DIFF_FATAL          = 2,
        EXIT_STATUS_NO_CHANGE           = 3,
};

#define ERRORX(format, ...) \
    errx(EXIT_STATUS_ERROR, "ERROR: %s: %s: %d: " format, childobj, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ERROR(format, ...)  do{ \
        printf("ERROR: %s\n", format); \
        exit(EXIT_STATUS_ERROR); \
    }while(0)\

#define PRINT_NEWLINE(format, ...) do{ \
    printf("%s\n", format); \
}while(0) \

#endif
