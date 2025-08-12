#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>
#include "db.h"

static sqlite3 *cache;

void initDB() {
    char *home = getenv("HOME");
    char db_path[256];
    snprintf(db_path, sizeof(db_path), "%s/.cbot_cache", home);

    if (sqlite3_open(db_path, &cache)) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(cache));
        return;
    }

    char *err_msg = 0;
    const char *sql_questions = "CREATE TABLE IF NOT EXISTS questions (id INTEGER PRIMARY KEY, question TEXT, answer TEXT, count INTEGER DEFAULT 1, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)";
    const char *sql_conversations = "CREATE TABLE IF NOT EXISTS conversations (id INTEGER PRIMARY KEY, messages TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)";
    const char *sql_agent_memory = "CREATE TABLE IF NOT EXISTS agent_memory (id INTEGER PRIMARY KEY, memory_item TEXT, timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)";

    if (sqlite3_exec(cache, sql_questions, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    if (sqlite3_exec(cache, sql_conversations, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    if (sqlite3_exec(cache, sql_agent_memory, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
}

void closeDB() {
    sqlite3_close(cache);
}

char *checkQ(const char *question_text) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT answer FROM questions WHERE question = ?";
    if (sqlite3_prepare_v2(cache, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(cache));
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, question_text, -1, SQLITE_STATIC);

    char *answer = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        answer = strdup((const char *)sqlite3_column_text(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return answer;
}

void insertQ(const char *question_text, const char *answer_text) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO questions (question, answer) VALUES (?, ?)";
    if (sqlite3_prepare_v2(cache, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(cache));
        return;
    }

    sqlite3_bind_text(stmt, 1, question_text, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, answer_text, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert question: %s\n", sqlite3_errmsg(cache));
    }

    sqlite3_finalize(stmt);

    // Insert message history into conversations table
    json_t *messages_array = json_array();
    json_t *user_message = json_object();
    json_object_set_new(user_message, "role", json_string("user"));
    json_object_set_new(user_message, "content", json_string(question_text));
    json_array_append_new(messages_array, user_message);

    json_t *assistant_message = json_object();
    json_object_set_new(assistant_message, "role", json_string("assistant"));
    json_object_set_new(assistant_message, "content", json_string(answer_text));
    json_array_append_new(messages_array, assistant_message);

    char *messages_str = json_dumps(messages_array, 0);

    sql = "INSERT INTO conversations (messages) VALUES (?)";
    if (sqlite3_prepare_v2(cache, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(cache));
        return;
    }

    sqlite3_bind_text(stmt, 1, messages_str, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert conversation: %s\n", sqlite3_errmsg(cache));
    }

    sqlite3_finalize(stmt);
    json_decref(messages_array);
    free(messages_str);
}

char **fetch_previous_prompts() {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT messages FROM conversations ORDER BY timestamp DESC LIMIT 10";
    if (sqlite3_prepare_v2(cache, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(cache));
        return NULL;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        count++;
    }

    char **prompts = malloc((count + 1) * sizeof(char *));
    sqlite3_reset(stmt);

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        prompts[i] = strdup((const char *)sqlite3_column_text(stmt, 0));
        i++;
    }
    prompts[i] = NULL;

    sqlite3_finalize(stmt);
    return prompts;
}

char **load_agent_memory() {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT memory_item FROM agent_memory ORDER BY timestamp ASC";
    if (sqlite3_prepare_v2(cache, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(cache));
        return NULL;
    }

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        count++;
    }

    char **memory = malloc((count + 1) * sizeof(char *));
    sqlite3_reset(stmt);

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        memory[i] = strdup((const char *)sqlite3_column_text(stmt, 0));
        i++;
    }
    memory[i] = NULL;

    sqlite3_finalize(stmt);
    return memory;
}

void save_agent_memory_item(const char *memory_item) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO agent_memory (memory_item) VALUES (?)";
    if (sqlite3_prepare_v2(cache, sql, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(cache));
        return;
    }

    sqlite3_bind_text(stmt, 1, memory_item, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert memory item: %s\n", sqlite3_errmsg(cache));
    }

    sqlite3_finalize(stmt);
}

void clear_agent_memory() {
    char *err_msg = 0;
    const char *sql = "DELETE FROM agent_memory";
    if (sqlite3_exec(cache, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
}

