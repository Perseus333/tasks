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
#define TASKS_TEMP_EXTENSION ".tmp"
#define TASKS_PARAMETER_COUNT 4
#define TASKS_BUFFER_LEN 5
#define TASKS_TIMESTAMP_LEN 10 // Currently UNIX (seconds)
#define TASKS_EXCLUDE_COMPLETE 1
#define TASKS_INCLUDE_ALL 0

struct Task {
    int is_complete;
    int completed_time;
    int created_time;
    char *name;
};

char *db_dir_path = NULL;
char *db_path = NULL;

struct Task *tasks;
int task_count;

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

// TODO: Make more efficient
FILE *open_db(const char *mode) {
    FILE* db = fopen(db_path, mode);
    if (!db) {
        db = fopen(db_path, "a");
        if (!db) {
            fprintf(stderr, "Could not open nor create tasks database.\n");
            exit(1);
        }
        fclose(db);
        db = fopen(db_path, mode);
    }
    return db;
}

/* Unused for now
void readable_timestamp(const long int unix_time, char* buffer, size_t size) {
    strftime(buffer, size, "%Y-%m-%d-%T%H:%M:%S", localtime(&unix_time));
}
*/

int count_tasks() {
    FILE *db = open_db("r");
    char *line = NULL;
    size_t len = 0;
    int task_count = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        task_count++;
    }

    return task_count;
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

int read_index() {
    int task_index;
    char buffer[TASKS_BUFFER_LEN];
    printf("Enter the task index: ");
    
    if (fgets(buffer, TASKS_BUFFER_LEN, stdin) == NULL) {
        perror("Input too long");
        return -1;
    }
    
    char *endptr;
    task_index = strtol(buffer, &endptr, 10);

    if ((errno == ERANGE) || (endptr == buffer) || (*endptr && *endptr != '\n')) {
        perror("Enter a valid integer");
        return -1;
    }

    if ((0 > task_index) || (task_index >= count_tasks())) {
        perror("Index does not correspond to any task");
        return -1;
    }
    return task_index;
}

void parse_tasks(int task_count) {
    FILE *db = open_db("r");
    char *line = NULL;
    size_t len = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        line[strcspn(line, "\n")] = 0;
        char *token = strtok(line, "|");
        char *parts[TASKS_PARAMETER_COUNT] = {0};

        for (int j = 0; token && j < TASKS_PARAMETER_COUNT; j++) {
            parts[j] = token;
            token = strtok(NULL, "|");
        }
        
        if (strcmp(parts[0], "[x]") == 0) {
            tasks[i].is_complete = 1;
        } else if (strcmp(parts[0], "[ ]") == 0) {
            tasks[i].is_complete = 0;
        } else {
            perror("Malformed DB, could not read completeness\n");
            exit(1);
        }

        char* endptr;
        tasks[i].completed_time = strtol(parts[1], &endptr, 10);
        tasks[i].created_time = strtol(parts[2], &endptr, 10);
        tasks[i].name = strdup(parts[3]);
    }

    free(line);
    fclose(db);
}

int list_tasks(int exclude_complete) {
    int index = 0;
    for (int i = 0; i < task_count; i++) {
        struct Task task = tasks[i];
        if ((tasks[i].is_complete == 0) || (exclude_complete == 0)) {
            char *complete_format;
            if (task.is_complete) {
                complete_format = "[x]";
            }
            else {
                complete_format = "[ ]";
            }
            printf("%d.\t%s\t%s\n", index, complete_format, task.name);
            index++;
        }
    }
    return 0;
}

int create_task(const char *task_name) {
    FILE *db = open_db("a");
    char timestamp[TASKS_TIMESTAMP_LEN+1]; // +1 for \0
    sprintf(timestamp, "%ld", time(NULL));
    // Format (based on todo.txt)
    // completed|completed_time|creation_time|name
    fprintf(db, "[ ]|");
    for (int i = 0; i < TASKS_TIMESTAMP_LEN; i++) {
        fprintf(db, "0");
    }
    fprintf(db, "|");
    fprintf(db, "%s", timestamp);
    fprintf(db, "|");
    fprintf(db, "%s\n", task_name);
    fclose(db);
    return 0;
}

int complete_task() {
    list_tasks(TASKS_EXCLUDE_COMPLETE);
    int task_index = read_index();
    if (task_index == -1) {
        return 1;
    }
    
    FILE *db = open_db("r");
    char temp_path[strlen(db_path)+strlen(TASKS_TEMP_EXTENSION)];
    snprintf(temp_path, sizeof(temp_path), "%s%s", db_path, TASKS_TEMP_EXTENSION);
    FILE *temp = fopen(temp_path, "w");
    if (!temp) return 1;

    char *line = NULL;
    size_t len = 0;
    int current_index = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        line[strcspn(line, "\n")] = '\0';

        char *parts[TASKS_PARAMETER_COUNT] = {0};
        char *token = strtok(line, "|");

        for (int j = 0; token && j < TASKS_PARAMETER_COUNT; j++) {
            parts[j] = token;
            token = strtok(NULL, "|");
        }

        if (current_index == task_index) {
            char timestamp[TASKS_TIMESTAMP_LEN+1]; // +1 for \0
            snprintf(timestamp, sizeof(timestamp), "%ld", time(NULL));
            fprintf(temp, "[x]|%s|%s|%s\n", timestamp, parts[2], parts[3]);
        } else {
            // Could be expanded to include a toggle
            fprintf(temp, "%s|%s|%s|%s\n", parts[0], parts[1], parts[2], parts[3]);
        }
        if (strcmp(parts[0], "[x]") != 0) {
            current_index++;
        }
    }

    free(line);
    fclose(db);
    fclose(temp);

    remove(db_path);
    rename(temp_path, db_path);
    return 0;
}

int delete_task() {
    list_tasks(TASKS_INCLUDE_ALL);
    int task_index = read_index();
    if (task_index == -1) {
        return 1;
    }

    FILE *db = open_db("r");
    char temp_path[strlen(db_path)+strlen(TASKS_TEMP_EXTENSION)];

    snprintf(temp_path, sizeof(temp_path), "%s.tmp", db_path);
    FILE *temp = fopen(temp_path, "w");
    if (!temp) return 1;

    char *line = NULL;
    size_t len = 0;
    int current_index = 0;

    while (getline(&line, &len, db) != -1) {
        // Only skip the selected task index
        if (current_index != task_index) {
            fputs(line, temp);
        }
        current_index++;
    }

    free(line);
    fclose(db);
    fclose(temp);

    if (task_index > task_count) {
        perror("Index provided was larger than task amount\n");
        return 1;
    }
    
    remove(db_path);
    rename(temp_path, db_path);
    return 0;
}

void print_help(const char *prog_name) {
    printf("Usage: %s [TASK | delete | complete | clear | -h]\n", prog_name);
    printf("  No arguments        List tasks\n");
    printf("  TASK                Add a new task\n");
    printf("  complete            Complete a task\n");
    printf("  delete              Delete a task\n");
    printf("  clearall            Remove all tasks\n");
    printf("  -h                  Show this help message\n");
}

int main(int argc, char **argv) {
    init_paths();
    ensure_db_dir();
    task_count = count_tasks();
    tasks = (struct Task *)malloc(sizeof(struct Task) * task_count);
    if (tasks == NULL) {
        perror("malloc");
        return 1;
    }
    parse_tasks(task_count);

    if (argc == 1) {
        return list_tasks(TASKS_EXCLUDE_COMPLETE);
    } else if (strcmp(argv[1], "complete") == 0) {
        return complete_task();
    } else if (strcmp(argv[1], "delete") == 0) {
        return delete_task();
    } else if (strcmp(argv[1], "clearall") == 0) {
        FILE *db = open_db("w");
        if (db) fclose(db);
    } else if (strcmp(argv[1], "-h") == 0) {
        print_help(argv[0]);
    } else {
        char *joined = join_argv(argc, argv, 1);
        create_task(joined);
        free(joined);
    }

    free(db_dir_path);
    free(db_path);
        for (int i = 0; i < task_count; i++) {
        free(tasks[i].name);
    }
    free(tasks);
    return 0;
}
