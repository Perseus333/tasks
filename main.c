#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define TT_DB_SUBDIR "/.local/share/tt"
#define TT_DB_FILE "/db.txt"

char db_dir_path[512];
char db_path[512];

void init_paths() {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Could not determine home directory.\n");
        exit(1);
    }

    snprintf(db_dir_path, sizeof(home) + sizeof(TT_DB_SUBDIR), "%s%s", home, TT_DB_SUBDIR);
    snprintf(db_path, sizeof(db_dir_path) + sizeof(TT_DB_FILE) , "%s%s", db_dir_path, TT_DB_FILE);
}

void ensure_db_dir() {
    struct stat st;
    if (stat(db_dir_path, &st) == -1) {
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", db_dir_path);
        system(cmd);
    }
}

FILE *open_db(const char *mode) {
    ensure_db_dir();
    return fopen(db_path, mode);
}

void readable_timestamp(const long int unix_time, char* buffer, size_t size) {
    strftime(buffer, size, "%Y-%m-%d-%T%H:%M:%S", localtime(&unix_time));
}

int list_tasks() {
    FILE *db = open_db("r");
    if (!db) return 1;
    char *line = NULL;
    size_t len = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        line[strcspn(line, "\n")] = 0;
        char *parts[3] = {0};
        char *token = strtok(line, "|");    
        
        // Change 3 depending on parameters for each line  
        for (int j = 0; token && j < 3; j++) {
            parts[j] = token;
            token = strtok(NULL, "|");
        }

        printf("%d.\t|%s|\t%s\n", i, parts[1], parts[2]);
    }

    free(line);
    fclose(db);
    return 0;
}

int create_task(const char *task) {
    FILE *db = open_db("a");
    if (!db) return 1;
    fprintf(db, "[ ]|"); // not completed
    char ts[20];
  //  sprintf(ts, "%ld", time(NULL));
    readable_timestamp(time(NULL), ts, sizeof("%Y-%m-%d-%T%H:%M:%S"));

    fprintf(db, "%s", ts);
    fprintf(db, "|");
    fprintf(db, "%s\n", task);
    fclose(db);
    return list_tasks();
}

int delete_task(int task_index) {
    FILE *db = open_db("r");
    if (!db) return 1;

    char temp_path[600];

    snprintf(temp_path, sizeof(temp_path), "%s.tmp", db_path);
    FILE *temp = fopen(temp_path, "w");
    if (!temp) return 1;

    char *line = NULL;
    size_t len = 0;
    for (int i = 0; getline(&line, &len, db) != -1; i++)
        if (i != task_index)
            fputs(line, temp);

    free(line);
    fclose(db);
    fclose(temp);

    remove(db_path);
    rename(temp_path, db_path);
    return list_tasks();
}



void print_help(const char *prog_name) {
    printf("Usage: %s [TASK | -d INDEX | clear | -h]\n", prog_name);
    printf("  No arguments        List tasks\n");
    printf("  TASK                Add a new task\n");
    printf("  -d INDEX            Delete task at index\n"); 
    printf("  clear               Remove all tasks\n");
    printf("  -h                  Show this help message\n");
}

int main(int argc, char **argv) {
    init_paths();

    if (argc == 1) {
        return list_tasks();
    } else if (strcmp(argv[1], "-d") == 0 && argc >= 3) {
        return delete_task(atoi(argv[2]));
    }  else if (strcmp(argv[1], "clear") == 0) {
        FILE *db = open_db("w");
        if (db) fclose(db);
    } else if (strcmp(argv[1], "-h") == 0) {
        print_help(argv[0]);
    } else {
        char task_name[50];
        for (int i = 1; i < argc; i++) {
            strcat(task_name, argv[i]);
            strcat(task_name, " ");
        }
        return create_task(task_name);
    }

    return 0;
}
