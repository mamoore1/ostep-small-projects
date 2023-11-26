
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include "helpers.h"

/* Enter the relative path of a directory, and receive a list of the files in
that directory that can be opened by the server. For simplicity, this list
currently includes all .txt files in that directory. Returns 0 if successful, 
otherwise returns -1 */
int get_openable_file_list(char *directory_name, char *filenames) {
    DIR *dp;
    if ((dp = opendir(directory_name)) == NULL) {
        printf("Failed to open directory");
        return -1;
    }

    struct dirent *d;
    // TODO: determine what to do if the directory is too big to fit in this
    strcpy(filenames, "\nAvailable files:\n");

    while ((d = readdir(dp)) != NULL) {
        if ((endswith(d->d_name, ".txt")) == 0) {
            strcat(filenames, d->d_name);
            strcat(filenames, "\n");
        }
    }
    closedir(dp);
    return 0;
}


// Check whether the input string ends with the comparison string. If the 
// comparison string is empty, return 0. Otherwise exhibits the same results as
// strncmp, i.e., 0 == a match
int endswith(char *string, char *comparison) {
    if (strcmp(string, "") == 0 || strcmp(comparison, "") == 0) {
        return 0;
    }

    // Get the final n characters, where n equals the lenght of the comparison string    
    int len_string = strlen(string);
    int len_comparison = strlen(comparison);

    if (len_comparison > len_string) {
        return 0; 
    }

    return strncmp(string + len_string - len_comparison, comparison, len_comparison);
}


/* H:TAoE */
void fatal(char *message) {
    char error_message[100];

    strcpy(error_message, "[!!] Fatal Error ");
    strncat(error_message, message, 83);
    exit(-1);
}
