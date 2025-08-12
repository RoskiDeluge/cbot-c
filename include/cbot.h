#ifndef CBOT_H
#define CBOT_H

// Options struct
typedef struct {
    char *model_name;
    int execute;
    int clip;
    int general;
    int shortcut;
    char *shortcut_name;
    char *shortcut_command;
    int history;
    int agent_mode;
} Options;

void run_cbot(int argc, char **argv);
Options *parse_options(int argc, char **argv);

#endif