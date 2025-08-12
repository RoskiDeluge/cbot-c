#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cbot.h"
#include "db.h"
#include "http.h"

Options *parse_options(int argc, char **argv) {
    Options *options = malloc(sizeof(Options));
    options->model_name = "llama3.2";
    options->execute = 0;
    options->clip = 0;
    options->general = 0;
    options->shortcut = 0;
    options->history = 0;
    options->agent_mode = 0;

    int opt;
    while ((opt = getopt(argc, argv, "l:do:s:axcgmh")) != -1) {
        switch (opt) {
            case 'l':
                if (strcmp(optarg, "32") == 0) {
                    options->model_name = "llama3.2";
                }
                break;
            case 'd':
                options->model_name = "deepseek-r1";
                break;
            case 'o':
                if (strcmp(optarg, "a") == 0) {
                    options->model_name = "openai-o4-mini";
                }
                break;
            case 'a':
                options->agent_mode = 1;
                break;
            case 'x':
                options->execute = 1;
                break;
            case 'c':
                options->clip = 1;
                break;
            case 'g':
                options->general = 1;
                break;
            case 's':
                options->shortcut = 1;
                options->shortcut_name = optarg;
                if (optind < argc) {
                    options->shortcut_command = argv[optind];
                    optind++;
                } else {
                    fprintf(stderr, "-s flag requires two arguments: shortcut name and command\n");
                    exit(1);
                }
                break;
            case 'm':
                options->history = 1;
                break;
            case 'h':
                printf("Cbot is a simple utility powered by AI (Ollama)\n");
                printf("\nExample usage:\n");
                printf("cbot how do I copy files to my home directory\n");
                printf("cbot \"How do I put my computer to sleep\"\n");
                printf("cbot -c \"how do I install homebrew?\"      (copies the result to clipboard)\n");
                printf("cbot -x what is the date                  (executes the result)\n");
                printf("cbot -g who was the 22nd president        (runs in general question mode)\n");
                printf("cbot -m                                   (prints the converstaion history)\n");
                printf("cbot -a                                   (runs in agent mode)\n");
                exit(0);
            default:
                fprintf(stderr, "Usage: %s [-l 32] [-d] [-o a] [-a] [-x] [-c] [-g] [-s] [-m] [-h] <question>\n", argv[0]);
                exit(1);
        }
    }

    return options;
}

void copy_to_clipboard(const char *text) {
    char command[1024];
#if __APPLE__
    snprintf(command, sizeof(command), "echo \"%s\" | pbcopy", text);
#elif __linux__
    snprintf(command, sizeof(command), "echo \"%s\" | xclip -selection clipboard", text);
#elif _WIN32
    snprintf(command, sizeof(command), "echo %s | clip", text);
#endif
    system(command);
}

void execute_command(const char *command) {
    char *start = strchr(command, '`');
    if (start) {
        char *end = strchr(start + 1, '`');
        if (end) {
            char extracted_command[1024];
            snprintf(extracted_command, end - start, "%s", start + 1);
            if (strstr(extracted_command, "sudo") != NULL) {
                printf("Execution canceled, cbot will not execute sudo commands.\n");
            } else {
                printf("cbot executing: %s\n", extracted_command);
                system(extracted_command);
            }
        } else {
            printf("Could not find closing backtick in command.\n");
        }
    } else {
        if (strstr(command, "sudo") != NULL) {
            printf("Execution canceled, cbot will not execute sudo commands.\n");
        } else {
            printf("cbot executing: %s\n", command);
            system(command);
        }
    }
}

void run_cbot(int argc, char **argv) {
    initDB();

    Options *options = parse_options(argc, argv);

    if (options->agent_mode) {
        printf("Entering agent mode. Type 'exit' to end the agent chat.\n");
        printf("Type 'clear' to clear conversation history.\n");

        char *line = NULL;
        size_t len = 0;
        ssize_t read;

        while ((read = getline(&line, &len, stdin)) != -1) {
            if (strcmp(line, "exit\n") == 0) {
                break;
            }
            if (strcmp(line, "clear\n") == 0) {
                clear_agent_memory();
                printf("Conversation history cleared.\n");
                continue;
            }

            char **memory = load_agent_memory();
            char *history = NULL;
            if (memory) {
                int total_len = 0;
                for (int i = 0; memory[i] != NULL; i++) {
                    total_len += strlen(memory[i]) + 1;
                }
                history = malloc(total_len);
                history[0] = '\0';
                for (int i = 0; memory[i] != NULL; i++) {
                    strcat(history, memory[i]);
                    strcat(history, "\n");
                    free(memory[i]);
                }
                free(memory);
            }

            ApiResponse api_response = call_model(line, history, options->model_name);
            if (api_response.success) {
                printf("Agent: %s\n", api_response.response);
                save_agent_memory_item(line);
                save_agent_memory_item(api_response.response);
                free(api_response.response);
            } else {
                printf("Failed to get answer from API\n");
            }

            if (history) {
                free(history);
            }
        }

        free(line);
    } else if (options->shortcut) {
        printf("Saving Shortcut\n");
        insertQ(options->shortcut_name, options->shortcut_command);
    } else if (options->history) {
        printf("CHAT HISTORY (last 10 messages):\n");
        char **prompts = fetch_previous_prompts();
        if (prompts) {
            for (int i = 0; prompts[i] != NULL; i++) {
                json_error_t error;
                json_t *root = json_loads(prompts[i], 0, &error);
                if (root) {
                    for (size_t j = 0; j < json_array_size(root); j++) {
                        json_t *message = json_array_get(root, j);
                        const char *role = json_string_value(json_object_get(message, "role"));
                        const char *content = json_string_value(json_object_get(message, "content"));
                        if (strcmp(role, "user") == 0) {
                            printf("User: %s\n", content);
                        } else if (strcmp(role, "assistant") == 0) {
                            printf("Assistant: %s\n\n", content);
                        }
                    }
                    json_decref(root);
                }
                free(prompts[i]);
            }
            free(prompts);
        }
    } else if (optind < argc) {
        char *question = argv[optind];
        char *answer = checkQ(question);

        if (answer) {
            printf("💾 Cache Hit\n");
            printf("%s\n", answer);
            if (options->clip) {
                copy_to_clipboard(answer);
            }
            if (options->execute) {
                execute_command(answer);
            }
            free(answer);
        } else {
            printf("-> Cache Miss\n");
            char *system_message = NULL;
            if (options->general) {
                system_message = "You are a helpful assistant. Answer the user's question in the best and most concise way possible.";
            } else {
#if __APPLE__
                system_message = "You are a command line translation tool for Mac. You will provide a concise answer to the user's question with the correct command.";
#elif __linux__
                system_message = "You are a command line translation tool for Linux. You will provide a concise answer to the user's question with the correct command.";
#elif _WIN32
                system_message = "You are a command line translation tool for Windows. You will provide a concise answer to the user's question with the correct command.";
#endif
            }

            ApiResponse api_response = call_model(question, system_message, options->model_name);
            if (api_response.success) {
                printf("%s\n", api_response.response);
                if (options->clip) {
                    copy_to_clipboard(api_response.response);
                }
                if (options->execute) {
                    execute_command(api_response.response);
                }
                insertQ(question, api_response.response);
                free(api_response.response);
            } else {
                printf("Failed to get answer from API\n");
            }
        }
    } else {
        // No question provided
    }

    free(options);
    closeDB();
}


