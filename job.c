// job.c
#include "job.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Job* addJob(Job** jobList, pid_t pid, char* command, char* status, int jobid) {
    Job* newJob = (Job*) malloc(sizeof(Job));
    if (!newJob) {
        perror("Failed to allocate memory for new job");
        return NULL;
    }
    newJob->pid = pid;
    char* copy_cmd = malloc(sizeof(char) * (strlen(command) + 1));
    for (int i = 0; i < strlen(command); i++) {
        copy_cmd[i] = command[i];
    }
    copy_cmd[strlen(command)] = '\0';
    newJob->command = copy_cmd;  // Duplicate the command string strdup(command);
    newJob->status = status;
    newJob->jobid = jobid;
    newJob->next = NULL;
    newJob->prev = NULL;  // Initialize prev to NULL

    // Add to back of the list
    if (*jobList == NULL) {  // If the list is empty
        *jobList = newJob;  // Make newJob the head
        (*jobList)->tail = newJob;  // And also the tail
    } else {
        newJob->prev = (*jobList)->tail;  // Set the previous of newJob
        (*jobList)->tail->next = newJob;  // Add newJob to the end
        (*jobList)->tail = newJob;  // Make newJob the new tail
    }

    return newJob;
}

void removeJob(Job** jobList, pid_t pid) {
    Job* current = *jobList;
    Job* previous = NULL;
    while (current) {
        if (current->pid == pid) {
            if (previous) {
                previous->next = current->next;
                if (current == (*jobList)->tail) {  // If current is tail
                    (*jobList)->tail = previous;  // Update the tail
                }
            } else {
                *jobList = current->next;
            }
            free(current->command);
            // free(current->status);
            free(current);
            return;
        }
        previous = current;
        current = current->next;
    }
}

Job* findJobByPid(Job* jobList, pid_t pid) {
    while (jobList) {
        if (jobList->pid == pid) {
            return jobList;
        }
        jobList = jobList->next;
    }
    return NULL;
}

pid_t findJobByJobid(Job* jobList, int jobid) {
    while (jobList) {
        if (jobList->jobid == jobid) {
            if (strcmp(jobList->status, "running") == 0) {
                return -1;
            }
            return jobList->pid;
        }
        jobList = jobList-> next;
    }
    return -1;
}


void print_jobs(Job* jobList) {
    if (jobList == NULL) {
		return;
	}
    Job* curr = jobList;
    while (curr != NULL) {
        int end = strlen(curr->command) - 1;
        while (end >= 0 && (curr->command[end] == '&' || isspace(curr->command[end]))) {
            end--;
        }

        char* copy_cmd = malloc(sizeof(char) * (end + 2));
        for (int i = 0; i < end + 1; i++) {
            copy_cmd[i] = curr->command[i];
        }
        copy_cmd[end + 1] = '\0';

        // printf("[%i] (%s) %s", curr->jobid, curr->status, curr->command);
        printf("[%i] %s (%s)\n", curr->jobid, copy_cmd, curr->status);
        free(copy_cmd);
		curr = curr->next;
    }
    return;
}

char* restartJob(Job* jobList, pid_t pid) {
    if (jobList == NULL) return NULL;
    jobList = jobList->tail;
    
     while (jobList) {
        if (jobList->pid == pid) {
            bool restart = (strcmp(jobList->status, "stopped") == 0);
            jobList->status = "running";
            if (restart) printf("Restarting: %s", jobList->command);
            else printf("\nRunning: %s", jobList->command);
            return jobList->command;
        }
        jobList = jobList->prev;
    }
    return NULL;
}

pid_t firstStoppedJob(Job* jobList) {
    while(jobList) {
        if (strcmp(jobList->status, "stopped") == 0) {
            return jobList->pid;
        }
        jobList = jobList-> next;
    }
    return -1;
}

pid_t fgGetStoppedJob(Job* jobList) {
    if (jobList == NULL) return -1;
    Job * end = jobList->tail;
    jobList = end;

    while(jobList) {
        if (strcmp(jobList->status, "stopped") == 0) {
            return jobList->pid;
        }
        jobList = jobList->prev;
    }
    if (end) return end->pid;
    return -1;
}


pid_t fgStartJob(Job* jobList, int jobid) {
    if (jobList == NULL) return -1;
    jobList = jobList->tail;

    while (jobList) {
        if (jobList->jobid == jobid) {
            if (strcmp(jobList->status, "stopped") == 0) {
                return jobList->pid;
            }
        }
        jobList = jobList->prev;
    }
    return -1;
}

bool fgFindJob(Job* jobList, int jobid) {
    if (jobList == NULL) return false;
    jobList = jobList->tail;

    while (jobList) {
        if (jobList->jobid == jobid) {
            return true;
        }
        jobList = jobList->prev;
    }
    return false;
}

pid_t fgGetJob(Job* jobList, int jobid) {
    if (jobList == NULL) return -1;
    jobList = jobList->tail;
    
    while (jobList) {
        if (jobList->jobid == jobid) {
            return jobList->pid;
        }
        jobList = jobList->prev;
    }
    return -1;
}

void stopJob(Job* jobList, pid_t pid) {
    while (jobList) {
        if (jobList->pid == pid) {
            jobList->status = "stopped";
            return;
        }
        jobList = jobList-> next;
    }
    return;
}

void print_fg_job(Job* jobList, pid_t pid) {
    while (jobList) {
        if (jobList->pid == pid) {
            printf("%s", jobList->command);
            return;
        }
        jobList = jobList-> next;
    }
    return;
}

void delete_list(Job** jobList) {
    /* delete entire list node by node */
	Job* curr = *jobList; 
	Job* next;
	while (curr != NULL) {
		next = curr->next;
        free(curr->command);
        // free(curr->status);
		free(curr);
		curr = next;
	}

	/* set head = NULL upon deletion */
	jobList = NULL;
	return ;
}

