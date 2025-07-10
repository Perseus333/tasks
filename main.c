#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

#define TASKS_DB_SUBDIR "/.local/share/tasks"
#define TASKS_DB_FILE "/db.txt"
#define TASKS_PARAMETER_COUNT 4
#define TASKS_NAME_LEN 100
#define TIMESTAMP_LEN 11 // Currently UNIX (seconds) + \0

char *db_dir_path = NULL;
char *db_path = NULL;

void init_paths() {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Could not determine home directory.\n");
        exit(1);
    }

    asprintf(&db_dir_path, "%s%s", home, TASKS_DB_SUBDIR);
    asprintf(&db_path, "%s%s", db_dir_path, TASKS_DB_FILE);
}

void ensure_db_dir() {
    if (mkdir(db_dir_path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
        exit(1);
    }
}

FILE *open_db(const char *mode) {
    ensure_db_dir();
    return fopen(db_path, mode);
}

void readable_timestamp(const long int unix_time, char* buffer, size_t size) {
    strftime(buffer, size, "%Y-%m-%d-%T%H:%M:%S", localtime(&unix_time));
}

char *join_argv(int argc, char **argv, int start_index) {
    size_t total_len = 1; // for \0

    for (int i = start_index; i < argc; i++) {
        total_len += strlen(argv[i]) + 1; // len + space
    }

    char *result = malloc(total_len);
    result[0] = '\0';

    for (int i = start_index; i < argc; i++) {
        strcat(result, argv[i]);
        strcat(result, " ");
    }
    
    return result;
}

int list_tasks() {
    FILE *db = open_db("r");
    if (!db) {
        db = open_db("a");
        if (!db) {
            fprintf(stderr, "Could not open tasks database.\n");
            return 1;
        }
        fclose(db);
        return 0;
    }
    char *line = NULL;
    size_t len = 0;
    int index = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        line[strcspn(line, "\n")] = 0;
        char *parts[TASKS_PARAMETER_COUNT] = {0};
        char *token = strtok(line, "|");    
        
        for (int j = 0; token && j < TASKS_PARAMETER_COUNT; j++) {
            parts[j] = token;
            token = strtok(NULL, "|");
        }
        // If not complete
        if (strcmp(parts[0], "[x]") != 0) {
            printf("%d.\t%s\n", index, parts[3]);
            index++;
        } 
    }

    free(line);
    fclose(db);
    return 0;
}

int create_task(const char *task_name) {
    FILE *db = open_db("a");
    if (!db) return 1;
    char timestamp[TIMESTAMP_LEN];
    sprintf(timestamp, "%ld", time(NULL));
    // Format (based on todo.txt)
    // completed|completed_time|creation_time|name
    fprintf(db, "[ ]|");
    for (int i = 0; i < TIMESTAMP_LEN-1; i++) {
        fprintf(db, "0");
    }
    fprintf(db, "|");
    fprintf(db, "%s", timestamp);
    fprintf(db, "|");
    fprintf(db, "%s\n", task_name);
    fclose(db);
    return list_tasks();
}

int complete_task(int task_index) {
    FILE *db = open_db("r");
    if (!db) return 1;

    char temp_path[_PC_PATH_MAX];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", db_path);
    FILE *temp = fopen(temp_path, "w");
    if (!temp) return 1;

    char *line = NULL;
    size_t len = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        if (i == task_index) {
            line[1] = 'x';

            char timestamp[TIMESTAMP_LEN];
            sprintf(timestamp, "%ld", time(NULL));
            const int OFFSET = 4;

            for (int j = 0; j < TIMESTAMP_LEN; j++) {
                line[j+OFFSET] = timestamp[j];
            }
        }
        fputs(line, temp);
    }

    free(line);
    fclose(db);
    fclose(temp);

    remove(db_path);
    rename(temp_path, db_path);
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

void cleanup() {
    free(db_dir_path);
    free(db_path);
}

void print_help(const char *prog_name) {
    printf("Usage: %s [TASK | -d INDEX | -c INDEX | clear | -h]\n", prog_name);
    printf("  No arguments        List tasks\n");
    printf("  TASK                Add a new task\n");
    printf("  -c INDEX            Complete task at index\n");
    printf("  -d INDEX            Delete task at index\n");
    printf("  clearall            Remove all tasks\n");
    printf("  -h                  Show this help message\n");
}

int main(int argc, char **argv) {
    init_paths();

    if (argc == 1) {
        return list_tasks();
    } else if (strcmp(argv[1], "-c") == 0 && argc >= 3) {
        return complete_task(atoi(argv[2]));
    } else if (strcmp(argv[1], "-d") == 0 && argc >= 3) {
        return delete_task(atoi(argv[2]));
    } else if (strcmp(argv[1], "clearall") == 0) {
        FILE *db = open_db("w");
        if (db) fclose(db);
    } else if (strcmp(argv[1], "-h") == 0) {
        print_help(argv[0]);
    } else {
        char *task_name = join_argv(argc, argv, 1);
        int result = create_task(task_name);
        free(task_name);
        cleanup();
        return result;
    }
    cleanup();
    return 0;
}
