// Simple thread pool for parallel test execution
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_TASKS 1000
#define MAX_WORKERS 50

typedef struct {
    char command[512];
    int result;
    int completed;
} Task;

static Task tasks[MAX_TASKS];
static int task_count = 0;
static int next_task = 0;
static int completed_tasks = 0;
static pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;
static int num_workers = 0;
static pthread_t worker_threads[MAX_WORKERS];

void* worker_thread(void* arg) {
    (void)arg;
    
    while (1) {
        pthread_mutex_lock(&task_mutex);
        
        if (next_task >= task_count) {
            pthread_mutex_unlock(&task_mutex);
            break;
        }
        
        int task_id = next_task++;
        pthread_mutex_unlock(&task_mutex);
        
        // Execute task using fork/exec for thread safety
        pid_t pid = fork();
        int result;
        
        if (pid == 0) {
            // Child process
            execl("/bin/sh", "sh", "-c", tasks[task_id].command, (char*)NULL);
            _exit(127);  // If execl fails
        } else if (pid > 0) {
            // Parent process
            int status;
            waitpid(pid, &status, 0);
            result = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        } else {
            // Fork failed
            result = -1;
        }
        
        pthread_mutex_lock(&task_mutex);
        tasks[task_id].result = result;
        tasks[task_id].completed = 1;
        completed_tasks++;
        pthread_mutex_unlock(&task_mutex);
    }
    
    return NULL;
}

// Add task to pool
int pool_add_task(const char* command) {
    if (task_count >= MAX_TASKS) {
        return -1;
    }
    
    strncpy(tasks[task_count].command, command, 511);
    tasks[task_count].command[511] = '\0';
    tasks[task_count].result = 0;
    tasks[task_count].completed = 0;
    task_count++;
    
    return task_count - 1;
}

// Start thread pool
void pool_start(int workers) {
    num_workers = workers > MAX_WORKERS ? MAX_WORKERS : workers;
    next_task = 0;
    completed_tasks = 0;
    
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&worker_threads[i], NULL, worker_thread, NULL);
    }
}

// Wait for all tasks
int pool_wait() {
    for (int i = 0; i < num_workers; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    
    // Count failures
    int failures = 0;
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].result != 0) {
            failures++;
        }
    }
    
    // Reset
    task_count = 0;
    next_task = 0;
    completed_tasks = 0;
    
    return failures;
}
