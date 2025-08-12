#ifndef DB_H
#define DB_H

#include <sqlite3.h>

void initDB();
void closeDB();
char *checkQ(const char *question_text);
void insertQ(const char *question_text, const char *answer_text);
char **fetch_previous_prompts();
char **load_agent_memory();
void save_agent_memory_item(const char *memory_item);
void clear_agent_memory();

#endif
