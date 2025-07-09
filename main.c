// Tasks
// Manage daily tasks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DB_DIR "/home/perseus/.local/share/tt"
#define DB_PATH "/home/perseus/.local/share/tt/db.txt"

void ensure_db_dir() {
    struct stat st;
    if (stat(DB_DIR, &st) == -1) {
        system("mkdir -p " DB_DIR);
    }
}

FILE *open_db(const char *mode) {
    ensure_db_dir();
    return fopen(DB_PATH, mode);
}

int list_tasks() {
    FILE *db = open_db("r");
    char *line = NULL;
    size_t len = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        printf("[%d] %s", i, line);
    }

    free(line);
    fclose(db);
    return 0;
}

int delete_task(int task_index) {
    FILE *db = open_db("r");
    FILE *temp = fopen(DB_PATH ".tmp", "w");
    
    char *line = NULL;
    size_t len = 0;
    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        if (i != task_index)
            fputs(line, temp);
    }

    free(line);
    fclose(db);
    fclose(temp);

    remove(DB_PATH);
    rename(DB_PATH ".tmp", DB_PATH);
    return list_tasks();
}

int create_task(const char *task) {
    FILE *db = open_db("a");
    fprintf(db, "%s\n", task);
    fclose(db);
    return list_tasks();
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return list_tasks();
    } 
    else if (strcmp(argv[1], "-d") == 0 && argc >= 3) {
        return delete_task(atoi(argv[2]));
    } else if (strcmp(argv[1], "clear") == 0) {
        FILE *db = open_db("w");
        fclose(db);
    } else {
        return create_task(argv[1]);
    }
}
