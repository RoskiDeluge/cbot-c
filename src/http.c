#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#include "http.h"

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

ApiResponse call_openai_model(const char *prompt, const char *system_message, const char *model) {
    CURL *curl;
    CURLcode res;
    ApiResponse api_response = { .response = NULL, .success = 0 };
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "https://api.openai.com/v1/chat/completions");

        json_t *payload_json = json_object();
        json_object_set_new(payload_json, "model", json_string(model));
        json_t *messages_array = json_array();
        if (system_message) {
            json_t *system_message_json = json_object();
            json_object_set_new(system_message_json, "role", json_string("system"));
            json_object_set_new(system_message_json, "content", json_string(system_message));
            json_array_append_new(messages_array, system_message_json);
        }
        json_t *user_message_json = json_object();
        json_object_set_new(user_message_json, "role", json_string("user"));
        json_object_set_new(user_message_json, "content", json_string(prompt));
        json_array_append_new(messages_array, user_message_json);
        json_object_set_new(payload_json, "messages", messages_array);

        char *payload_str = json_dumps(payload_json, 0);

        char *api_key = getenv("OPENAI_API_KEY");
        if (!api_key) {
            fprintf(stderr, "OPENAI_API_KEY environment variable not set\n");
            return api_response;
        }

        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, auth_header);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            json_error_t error;
            json_t *root = json_loads(chunk.memory, 0, &error);

            if (root) {
                json_t *choices_array = json_object_get(root, "choices");
                if (json_is_array(choices_array) && json_array_size(choices_array) > 0) {
                    json_t *first_choice = json_array_get(choices_array, 0);
                    json_t *message = json_object_get(first_choice, "message");
                    json_t *content = json_object_get(message, "content");
                    if (json_is_string(content)) {
                        api_response.response = strdup(json_string_value(content));
                        api_response.success = 1;
                    }
                }
                json_decref(root);
            }
        }

        curl_easy_cleanup(curl);
        free(payload_str);
        json_decref(payload_json);
        free(chunk.memory);
    }

    curl_global_cleanup();
    return api_response;
}

ApiResponse call_model(const char *prompt, const char *system_message, const char *model) {
    if (strstr(model, "openai") != NULL) {
        return call_openai_model(prompt, system_message, model);
    }

    CURL *curl;
    CURLcode res;
    ApiResponse api_response = { .response = NULL, .success = 0 };
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl) {
        char url[256];
        snprintf(url, sizeof(url), "http://localhost:11434/api/generate");

        json_t *payload_json = json_object();
        json_object_set_new(payload_json, "model", json_string(model));
        json_object_set_new(payload_json, "prompt", json_string(prompt));
        json_object_set_new(payload_json, "stream", json_false());

        if (system_message) {
            json_object_set_new(payload_json, "system", json_string(system_message));
        }

        char *payload_str = json_dumps(payload_json, 0);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            json_error_t error;
            json_t *root = json_loads(chunk.memory, 0, &error);

            if (root) {
                json_t *response_json = json_object_get(root, "response");
                if (json_is_string(response_json)) {
                    api_response.response = strdup(json_string_value(response_json));
                    api_response.success = 1;
                }
                json_decref(root);
            }
        }

        curl_easy_cleanup(curl);
        free(payload_str);
        json_decref(payload_json);
        free(chunk.memory);
    }

    curl_global_cleanup();
    return api_response;
}