// Tasks
// Manage daily tasks

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_PATH "/home/perseus/.local/share/tt/db.txt"

int list_tasks() {
    FILE *file = fopen(DB_PATH, "r");
    if (!file) {
        system("mkdir -p ~/.local/share/tt");
        file = fopen(DB_PATH, "wb");
        if (!file) {
            perror("Failed to create DB");
            return -1;
        }
        fclose(file);
        printf("Created new DB\n");
        return 0;
    }

    char *line = NULL;
    size_t len = 0;
    size_t read;
    int index = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        printf("[%d] %s", index, line);
        index++;
    }

    free(line);
    fclose(file);
    return 0;
}

int delete_task(int task_index) {
    FILE *file = fopen(DB_PATH, "r");
    if (!file) {
        perror("Could not open db.txt");
        return -1;
    }

    FILE *tmp = fopen("tmp.txt", "w");
    if (!tmp) {
        perror("Could not open temp file");
        fclose(file);
        return -1;
    }

    char *line = NULL;
    size_t len = 0;
    size_t nread;
    int line_num = 0;

    while ((nread = getline(&line, &len, file)) != -1) {
        if (line_num != task_index) {
            fwrite(line, 1, nread, tmp);
        }
        line_num++;
    }

    free(line);
    fclose(file);
    fclose(tmp);

    // Replace original file with the temp
    remove(DB_PATH);
    rename("tmp.txt", DB_PATH);
    list_tasks();    
    return 0;
}

int create_task(const char *task) {
    FILE *file = fopen(DB_PATH, "a");
    if (!file) {
        perror("Unable to open db.txt");
        return -1;
    }

    fprintf(file, "%s\n", task);
    fclose(file);
    list_tasks();
    return 0;
}

int clear_tasks() {
    fopen(DB_PATH, "w");
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return list_tasks();
    } 
    else {
        if (strcmp(argv[1], "-d") == 0 && argc >= 3) {
            return delete_task(atoi(argv[2]));
        } else if (strcmp(argv[1], "clear") == 0) {
            return clear_tasks();
        } else {
            return create_task(argv[1]);
        }
    }
}
