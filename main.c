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
#define TASKS_BACKUP_EXTENSION ".bak"
#define TASKS_BUFFER_LEN 5
#define TASKS_TIMESTAMP_LEN 10 // Currently UNIX (seconds)
#define TASKS_INCLUDE_ALL 0
#define TASKS_ONLY_PENDING 1

enum {
    TASKS_IS_COMPLETE_INDEX = 0,
    TASKS_COMPLETED_INDEX,
    TASKS_CREATED_INDEX,
    TASKS_NAME_INDEX,
    TASKS_PARAMETER_COUNT
};

struct Task {
    int is_complete;
    int completed_time;
    int created_time;
    char *name;
    int file_line;
};

char *db_dir_path = NULL;
char *db_path = NULL;

struct Task *tasks;
int line_count;
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
    errno = 0;
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

int count_lines() {
    FILE *db = open_db("r");
    char *line = NULL;
    size_t len = 0;
    int line_count = 0;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        line_count++;
    }
    free(line);
    fclose(db);
    return line_count;
}

char *join_argv(int argc, char **argv, int start_index) {
    size_t total_len = 1;

    for (int i = start_index; i < argc; i++) {
        total_len += strlen(argv[i]);
    }
    total_len += (argc - start_index); // spaces

    char *result = malloc(total_len);
    result[0] = '\0';

    for (int i = start_index; i < argc; i++) {
        strcat(result, argv[i]);
        if (i != argc-1) {
            strcat(result, " ");
        }
    }

    return result;
}

int read_index() {
    int task_index;
    char buffer[TASKS_BUFFER_LEN];
    while (1) {
        printf("Enter the task index: ");

        if (fgets(buffer, TASKS_BUFFER_LEN, stdin) == NULL) {
            fprintf(stderr, "Failed to read input\n");
            continue;
        }

        if (buffer[0] == '\n') {
            printf("Canceled.\n");
            return -1;
        }
        
        if (strchr(buffer, '\n') == NULL) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF); // Discard remaining chars
            fprintf(stderr, "Input too long, try again\n");
            continue;
        }
        
        char *endptr;
        errno = 0;
        task_index = strtol(buffer, &endptr, 10);
    
        if ((errno == ERANGE) || (endptr == buffer) || (*endptr && *endptr != '\n')) {
            fprintf(stderr, "Enter a valid integer\n");
            continue;
        }
    
        if ((0 > task_index) || (task_index >= task_count)) {
            fprintf(stderr, "Index does not correspond to any valid task\n");
            continue;
        }

        return task_index;
    }
}

void parse_tasks(int line_count) {
    FILE *db = open_db("r");
    char *line = NULL;
    size_t len = 0;
    int index = 0;
    
    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        if (line != NULL) {
            line[strcspn(line, "\n")] = 0;
            char *token = strtok(line, "|");    
            char *parts[TASKS_PARAMETER_COUNT] = {0};
            int token_count = 0;

            while (token && token_count < TASKS_PARAMETER_COUNT) {
                parts[token_count++] = token;
                token = strtok(NULL, "|");
            }

            if (token_count < TASKS_PARAMETER_COUNT) {
                printf("Line %d is missing %d parameter(s)\n", i, TASKS_PARAMETER_COUNT - token_count);
                continue;
            }

            int errors = 0;

            if (strcmp(parts[TASKS_IS_COMPLETE_INDEX], "[x]") == 0) {
                tasks[index].is_complete = 1;
            } else if (strcmp(parts[TASKS_IS_COMPLETE_INDEX], "[ ]") == 0) {
                tasks[index].is_complete = 0;
            } else {
                printf("Could not read completeness of task at line %d\n", i);
                errors++;
            }

            char* endptr;
            errno = 0;

            int completed_time = strtol(parts[TASKS_COMPLETED_INDEX], &endptr, 10);
            if ((errno == ERANGE) || (endptr == parts[TASKS_COMPLETED_INDEX]) || (*endptr != '\0')) {
                printf("Completed time is not a valid UNIX timestamp at line %d\n", i);
                errors++;
            }
            errno = 0;
            int created_time = strtol(parts[TASKS_CREATED_INDEX], &endptr, 10);
            if ((errno == ERANGE) || (endptr == parts[TASKS_CREATED_INDEX]) || (*endptr != '\0')) {
                printf("Created time is not a valid UNIX timestamp at line %d\n", i);
                errors++;
            }
            if (errors > 0) {
                continue;
            }

            tasks[index].completed_time = completed_time;
            tasks[index].created_time = created_time;
            tasks[index].name = strdup(parts[TASKS_NAME_INDEX]);
            tasks[index].file_line = i;
            index++;
        }
    }
    task_count = index;
    free(line);
    fclose(db);
}

int list_tasks(int only_pending) {
    int index = 0;
    for (int i = 0; i < task_count; i++) {
        struct Task task = tasks[i];
        if (only_pending && task.is_complete) {
            continue;
        }
        printf("%d.\t%s\t%s\n",
            index,
            task.is_complete ? "[x]" : "[ ]",
            task.name
        );
    }
    return 0;
}

int create_task(const char *task_name) {
    FILE *db = open_db("a");
    char current_time[TASKS_TIMESTAMP_LEN+1];
    sprintf(current_time, "%ld", time(NULL));
    current_time[-1] = '\0';

    // Format (based on todo.txt)
    // is_completed|completed_time|creation_time|name
    char zero_timestamp[TASKS_TIMESTAMP_LEN+1];
    memset(zero_timestamp, '0', TASKS_TIMESTAMP_LEN);
	zero_timestamp[-1] = '\0';
    fprintf(db, "[ ]|%s|%s|%s\n", 
        zero_timestamp, 
        current_time,
        task_name);
    fclose(db);
    return 0;
}

int complete_task() {
    list_tasks(TASKS_INCLUDE_ALL);
    int task_index = read_index();
    if (task_index == -1) {
        return 1;
    }
    
    FILE *db = open_db("r");

    char temp_path[strlen(db_path)+strlen(TASKS_TEMP_EXTENSION)+1];
    snprintf(temp_path, sizeof(temp_path), "%s%s", db_path, TASKS_TEMP_EXTENSION);
    FILE *temp = fopen(temp_path, "w");
    if (!temp) return 1;

    char backup_path[strlen(db_path)+strlen(TASKS_BACKUP_EXTENSION)+1];
    snprintf(backup_path, sizeof(backup_path), "%s%s", db_path, TASKS_BACKUP_EXTENSION);
    FILE *backup = fopen(backup_path, "w");
    if (!backup) return 1;    

    char *line = NULL;
    size_t len = 0;
    int line_to_complete = tasks[task_index].file_line;

    for (int i = 0; getline(&line, &len, db) != -1; i++) {
        char *original_line = strdup(line);
        line[strcspn(line, "\n")] = '\0';

        char *parts[TASKS_PARAMETER_COUNT] = {0};
        char *token = strtok(line, "|");

        for (int j = 0; token && j < TASKS_PARAMETER_COUNT; j++) {
            parts[j] = token;
            token = strtok(NULL, "|");
        }

        if (i == line_to_complete) {
            char timestamp[TASKS_TIMESTAMP_LEN+1];
            snprintf(timestamp, sizeof(timestamp), "%ld", time(NULL));
            // Could be modified to toggle is_complete state
            fprintf(temp, "[x]|%s|%s|%s\n",
                timestamp, 
                parts[TASKS_CREATED_INDEX], 
                parts[TASKS_NAME_INDEX]);
        } else {
            fprintf(temp, "%s", original_line);
        }
        free(original_line);
    }

    free(line);
    fclose(db);
    fclose(temp);

    rename(db_path, backup_path);
    rename(temp_path, db_path);
    remove(backup_path);
    return 0;
}

int delete_task() {
    list_tasks(TASKS_INCLUDE_ALL);
    int task_index = read_index();
    if (task_index == -1) {
        return 1;
    }
    int line_to_delete = tasks[task_index].file_line;

    FILE *db = open_db("r");

    char temp_path[strlen(db_path)+strlen(TASKS_TEMP_EXTENSION)+1];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", db_path);
    FILE *temp = fopen(temp_path, "w");
    if (!temp) return 1;

    char backup_path[strlen(db_path)+strlen(TASKS_BACKUP_EXTENSION)+1];
    snprintf(backup_path, sizeof(backup_path), "%s%s", db_path, TASKS_BACKUP_EXTENSION);
    FILE *backup = fopen(backup_path, "w");
    if (!backup) return 1;    

    char *line = NULL;
    size_t len = 0;
    int current_line = 0;

    while (getline(&line, &len, db) != -1) {
        if (current_line != line_to_delete) {
            fputs(line, temp);
        }
        current_line++;
    }

    free(line);
    fclose(db);
    fclose(temp);
    
    rename(db_path, backup_path);
    rename(temp_path, db_path);
    remove(backup_path);
    return 0;
}

void print_help(const char *prog_name) {
    printf("Usage: %s [TASK | delete | complete | clear | -h]\n", prog_name);
    printf("  No arguments        List pending tasks\n");
    printf("  list                Lists all tasks\n");
    printf("  TASK                Add a new task\n");
    printf("  complete            Complete a task\n");
    printf("  delete              Delete a task\n");
    printf("  clearall            Remove all tasks\n");
    printf("  -h                  Show this help message\n");
}

int main(int argc, char **argv) {
    init_paths();
    ensure_db_dir();
    line_count = count_lines();
    errno = 0;
    tasks = (struct Task *)malloc(sizeof(struct Task) * line_count);
    if (tasks == NULL) {
        perror("malloc");
        return 1;
    }
    parse_tasks(line_count);

    if (argc == 1) {
        return list_tasks(TASKS_ONLY_PENDING);
    } else if (strcmp(argv[1], "list") == 0) {
        return list_tasks(TASKS_INCLUDE_ALL);
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
