#ifndef HTTP_H
#define HTTP_H

typedef struct {
    char *response;
    int success;
} ApiResponse;

ApiResponse call_model(const char *prompt, const char *system_message, const char *model);

#endif
