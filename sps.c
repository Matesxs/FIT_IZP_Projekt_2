/**
 * @version V1
 * @file sps.c
 * @author Martin Douša
 * @date November 2020
 * @brief Program to process tables from input file
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stdbool.h"

#define DEBUG
#define BLACKLISTED_DELIMS "\"\\" /**< Character that are not allowed to use as delim character */

#define BASE_NUMBER_OF_ROWS 4
#define BASE_NUMBER_OF_CELLS 4
#define BASE_CELL_LENGTH 16

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
    unsigned long long int allocated_chars;
    char *content; /**< Raw content of single cell */
} Cell;

/**
 * @struct Row
 * @brief Store for data of single row
 */
typedef struct
{
    unsigned long long int num_of_cells; /**< Number of cells in row */
    unsigned long long int allocated_cells; /**< Number of cell pointers allocated in memory */
    Cell *cells; /**< Pointer to first cell in row */
} Row;

/**
 * @struct Table
 * @brief Store for data of whole table
 */
typedef struct
{
    unsigned long long int num_of_rows; /**< Number of rows in table */
    unsigned long long int allocated_rows; /**< Number of row pointers allocated in memory */
    Row *rows; /**< Pointer to first row in table */
    char delim; /**< Delimiter for output */
} Table;

void deallocate_table(Table *table)
{
    /**
     * @brief Dealocate whole table from memory
     *
     * Iterate over whole every cell in every row, dealocate data and clear pointers
     *
     * @param table Pointer to instance of table structure
     */

    if (table->rows == NULL)
        return;

    for (unsigned long long int i = 0; i < table->num_of_rows; i++)
    {
        if (table->rows[i].cells == NULL)
            continue;

        for (unsigned long long int j = 0; j < table->rows[i].num_of_cells; j++)
        {
            if (table->rows[i].cells[j].content == NULL)
                continue;

            free(table->rows[i].cells[j].content);
            table->rows[i].cells[j].content = NULL;
        }

        free(table->rows[i].cells);
        table->rows[i].cells = NULL;
    }

    free(table->rows);
    table->rows = NULL;
}

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

unsigned long long int count_char(char *string, char c, _Bool ignore_escapes)
{
    /**
     * @brief Count specific character in string
     *
     * Count ocurences of specific character in string with option to ignore escaped characters and parts in parentecies
     *
     * @param string Input string
     * @param c Character we want to search
     * @param ignore_escapes Flag if we want ignore if characte is escaped or in parentecies and count it too
     *
     * @return Number of ocurences of searched character
     */

    if (string == NULL)
        return 0;

    size_t lenght_of_string = strlen(string);
    _Bool in_parentecies = false;
    unsigned long long int counter = 0;

    for (size_t i = 0; i < lenght_of_string; i++)
    {
        char cc = string[i];

        if (cc == '"')
            in_parentecies = !in_parentecies;

        if (cc == c)
        {
            if (ignore_escapes || (!in_parentecies && (i != 0 && string[i-1] != '\\')))
            {
                counter++;
            }
        }
    }

    return counter;
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

long long int get_line(char **line_buffer, FILE *file)
{
    /**
     * @brief Load one line from file
     *
     * Load one line from file and save it to buffer
     *
     * @param line_buffer Empty pointer to char pointer where will be saved loaded line
     * @param file Pointer to file from which will be line loaded
     *
     * @return Number of alloacated bits in memory or -1 when there is nothing else to load
     */

    int c;
    long long int index = 0;
    long long int allocated = 16;

    *line_buffer = NULL;

    *line_buffer = (char*)malloc(allocated);
    if (!*line_buffer)
        return 0;

    while ((c = fgetc(file)) != '\n' && c != EOF)
    {
        // Save loaded char to char array
        (*line_buffer)[index++] = (char)c;

        // If there is no space in allocated array try to reallocate it to larger array
        // +1 for end string bit
        if (index + 1 >= allocated)
        {
            char *tmp = (char*)realloc (*line_buffer, allocated + 16);

            // If reallocating failed free already used memory and return
            if (!tmp)
            {
                free(*line_buffer);
                *line_buffer = NULL;
                return 0;
            }

            *line_buffer = tmp;
            allocated += 16;
        }
    }

    // To the end add end string bit
    (*line_buffer)[index] = 0;

    // If we are here and no char is saved and current char in buffer is eof then there is nothing to load
    if (index == 0 && c == EOF) {
        free (*line_buffer);
        *line_buffer = NULL;
        return -1;
    }

    return allocated;
}

void normalize_delims(char *line, const char *delims)
{
    /**
     * @brief Normalize all delims to the first one
     *
     * Iterate over all delims and all characters in line and replace and valid delim from delims array in line by the first delim in delims array
     *
     * @param line Line string
     * @param delims Array of deliminator characters
     */

    size_t num_of_delims = strlen(delims);
    size_t length_of_line = strlen(line);

    for (size_t i = 1; i < num_of_delims; i++)
    {
        _Bool in_parentecies = false;

        for (size_t j = 0; j < length_of_line; j++)
        {
            char cc = line[j];

            if (cc == '"')
                in_parentecies = !in_parentecies;

            if (cc == delims[i])
            {
                if (!in_parentecies && (j != 0 && line[j-1] != '\\'))
                {
                    line[j] = delims[0];
                }
            }
        }
    }
}

int load_table(const char *delims, char *filepath, Table *table)
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

    int ret_val = NO_ERROR;
    FILE *file;
    char *line = NULL;
    unsigned long long int line_index = 0;

    // Try to open input file
    file = fopen(filepath, "r");
    if (file == NULL)
        return FILE_DOESNT_EXIST;

    // Allocate first row
    table->rows = (Row*)malloc(BASE_NUMBER_OF_ROWS * sizeof(Row));
    if (table->rows == NULL)
        return ALLOCATION_FAILED;

    table->num_of_rows = 0;
    table->allocated_rows = BASE_NUMBER_OF_ROWS;

//    table->num_of_rows = 1;
//    table->rows[0].cells = (Cell*)malloc(sizeof(Cell));
//    if (table->rows[0].cells == NULL)
//    {
//        deallocate_table(table);
//        return ALLOCATION_FAILED;
//    }
//
//    table->rows[0].num_of_cells = 0;
//    table->rows[0].allocated_cells = 1;

    // Iterate thru input file
    // When we set buffer size to 0 and output char pointer to NULL then getline will allocate memory itself
    while (get_line(&line, file) != -1)
    {
        if (line == NULL)
        {
            ret_val = ALLOCATION_FAILED;
            deallocate_table(table);
            break;
        }

        rm_newline_chars(line);

        normalize_delims(line, delims);

        // Check if we still have room in rows array
        if (line_index >= table->allocated_rows)
        {
            // Allocate larger array of rows
            Row *tmp = (Row*)realloc(table->rows, (table->allocated_rows + BASE_NUMBER_OF_ROWS) * sizeof(Row));
            if (tmp == NULL)
                return ALLOCATION_FAILED;

            table->allocated_rows += BASE_NUMBER_OF_ROWS;
            table->rows = tmp;
        }

        printf("%s -- %llu\n", line, count_char(line, table->delim, false));

        free(line);
        line_index++;
    }


    // Close input file
    fclose(file);

    return ret_val;
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

    Table table = { .delim = delims[0] };
    if ((error_flag = load_table(delims, argv[argc-1], &table)) != NO_ERROR)
    {
        fprintf(stderr, "Failed to load table properly\n");
        deallocate_table(&table);
        return error_flag;
    }

    if (table.rows == NULL)
    {
        fprintf(stderr, "Failed to load table correctly!\n");
        deallocate_table(&table);
        return TABLE_LOAD_ERROR;
    }

#ifdef DEBUG
    printf("\n\nDebug:\n");
    printf("Allocated rows: %llu\n", table.allocated_rows);
    printf("Delim: '%c'\n", delims[0]);
    printf("Args: ");
    for (int i = 1; i < argc; i++)
    {
        printf("%s ", argv[i]);
    }
    printf("\n");
#endif

    deallocate_table(&table);

    return NO_ERROR;
}
