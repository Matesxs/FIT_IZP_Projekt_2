#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stdbool.h"

#define BLACKLISTED_DELIMS "\"\'\\" /**< Character that are not allowed to use as delim character */

/**
 * @enum ErrorCodes
 * @brief Flags that will be returned on error
 */
enum ErrorCodes
{
    NO_ERROR,                     /**< No error detected (default return code) */
    MISSING_ARGS,                 /**< Some arguments are missing */
    INVALID_DELIMITER,            /**< Found invalid character in delimiters from arguments */
    TABLE_LOAD_ERROR,             /**< Data load error */
    FILE_DOESNT_EXIST,            /**< Error if input file doesnt exist or is used by another process */
    ALLOCATION_FAILED,             /**< Error when program failed to allocate memory */
};

/**
 * @struct Cell
 * @brief Store for data of single cell
 */
typedef struct
{
    char *content; /**< Raw content of single cell */
} Cell;

/**
 * @struct Row
 * @brief Store for data of single row
 */
typedef struct
{
    int num_of_cells; /**< Number of cells in row */
    Cell *cells; /**< Pointer to first cell in row */
} Row;

/**
 * @struct Table
 * @brief Store for data of whole table
 */
typedef struct
{
    int num_of_rows; /**< Number of rows in table */
    Row *rows; /**< Pointer to first row in table */
} Table;

_Bool strings_equal(const char *s1, const char *s2)
{
    /**
     * @brief Check if two string @p s1 and @p s2 are same
     *
     * @param s1 First string
     * @param s2 Second string
     *
     * @return true if string are equal, false if dont
     */

    return strcmp(s1, s2) == 0;
}

void rm_newline_chars(char *s) {
    /**
     * @brief Removing new line character from string
     *
     * Iterate over string until it new line character then replace it with 0
     * @warning
     * Rest of the string is REMOVED!
     *
     * @param s Pointer to string from which is new line character removed
     */

    while(*s && *s != '\n' && *s != '\r')
        s++;

    *s = 0;
}

int load_table(char *delims, char *filepath, Table *table)
{
    /**
     * @brief Load table from file
     *
     * Load and parse data from file
     *
     * @param delims Array with all posible delimiters
     * @param filepath Path to input file
     * @param table Pointer table where data will be saved
     *
     * @return NO_ERROR if table data are loaded and parsed properly, in other scenarios return error code
     */

    FILE *file;
    char *line = NULL;
    size_t buffer_size = 0;

    // Try to open input file
    printf("%s\n", filepath);
    file = fopen(filepath, "r");
    if (!file)
        return FILE_DOESNT_EXIST;

    // Iterate thru input file
    // When we set buffer size to 0 and output char pointer to NULL then getline will allocate memory itself
    while (getline(&line, &buffer_size, file) != -1)
    {
        if (line == NULL)
            return ALLOCATION_FAILED;

        rm_newline_chars(line);

        printf("%s - %ld\n", line, strlen(line));

        // Free line loaded by getline
        free(line);
    }


    // Close input file
    fclose(file);

    return NO_ERROR;
}

_Bool check_sanity_of_delims(char *delims)
{
    /**
     * @brief Check delims
     *
     * Check if in delims are no blacklisted characters
     *
     * @param delims Array of deliminators
     *
     * @return true if deliminator are without blacklisted characters else false
     */

    size_t length_of_delims = strlen(delims);
    size_t length_of_blacklisted_delims = strlen(BLACKLISTED_DELIMS);

    for (size_t i = 0; i < length_of_delims; i++)
    {
        for (size_t j = 0; j < length_of_blacklisted_delims; j++)
        {
            if (delims[i] == BLACKLISTED_DELIMS[j])
                return false;
        }
    }

    return true;
}

_Bool load_delims_from_args(char *argv[], char **delims)
{
    /**
     * @brief Load delims
     *
     * Check if second argument is -d flag and then set next argument as delims string
     *
     * @param argv Array of arguments
     * @param delims pointer to array of delims
     *
     * @return true if flag not found or delims after flag are valid, false if there are invalid characters in delims
     */

    if (strings_equal(argv[1], "-d"))
    {
        *delims = argv[2];
        if (!check_sanity_of_delims(*delims))
            return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    /**
     * @brief Main of whole program
     * @todo Refactor error handling
     */

    // Check if number of arguments is larger than minimum posible number of arguments
    if (argc < 3) {
        fprintf(stderr, "Some arguments are missing!\n");
        return MISSING_ARGS;
    }

    char *delims = " "; //**< Number of all posible delimiters to use */
    // Check if there is -d flag in arguments and load it if yes
    if (!load_delims_from_args(argv, &delims))
    {
        fprintf(stderr, "Cant found valid delimiters after -d flag\n");
        return INVALID_DELIMITER;
    }

    int error_flag;

    Table table;
    if ((error_flag = load_table(delims, argv[argc-1], &table)) != NO_ERROR)
    {
        fprintf(stderr, "Failed to load table properly\n");
        return error_flag;
    }

    if (table.rows == NULL)
    {
        fprintf(stderr, "Failed to load table correctly!\n");
        return TABLE_LOAD_ERROR;
    }

    return NO_ERROR;
}
