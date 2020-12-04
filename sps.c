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
#include <stdbool.h>
#include <float.h>

#define DEBUG

#define BLACKLISTED_DELIMS "\'\"\\" /**< Character that are not allowed to use as delim character */
#define DEFAULT_DELIM " " /**< Default delimiter array */

#define BASE_NUMBER_OF_ROWS 3 /**< Base number of rows that will be allocated */
#define BASE_NUMBER_OF_CELLS 3 /**< Base number of cells that will be allocated */
#define BASE_CELL_LENGTH 6 /**< Base length of content in cell that will be allocated */

#define NUMBER_OF_TEMPORARY_VARIABLES 10 /**< Number of how much temporarz variables should be allocated/used */

#define EMPTY_CELL "" /**< How should look like empty cell */

const char *TABLE_EDITING_COMMANDS[] = { "irow", "arow", "drow", "icol", "acol", "dcol" };         /**< Spreadsheet with table editing commands */
#define NUMBER_OF_TABLE_EDITING_COMMANDS 6                                                         /**< Number of table editing commands for iterating over array */
const char *DATA_EDITING_COMMANDS[] = { "set", "clear", "swap", "sum", "avg", "count", "len" };    /**< Spreadsheet with data editing commands */
#define NUMBER_OF_DATA_EDITING_COMMANDS 7                                                          /**< Number of data editing commands for iterating over array */
const char *TEMP_VAR_COMMANDS[] = { "def", "use", "inc" };                                         /**< Spreadsheet with temporary variable commands */
#define NUMBER_OF_TEMP_VAR_COMMANDS 3                                                              /**< Number of temporary variable commands for iterating over array */

/**
 * @enum CommandType
 * @brief Flags to indicate what type of command is currently executed
 */
enum CommandType
{
    TABLE_EDITING_COMMAND,
    DATA_EDITING_COMMAND,
    TEMP_VAR_COMMAND,
    UNKNOWN,
};

/**
 * @enum ErrorCodes
 * @brief Flags that will be returned on error
 */
enum ErrorCodes
{
    NO_ERROR,                     /**< No error detected (default return code) - 0 */
    MISSING_ARGS,                 /**< Some arguments are missing - 1 */
    INVALID_DELIMITER,            /**< Found invalid character in delimiters from arguments - 2 */
    CANT_OPEN_FILE,               /**< Error if given file cant be opened  - 3 */
    ALLOCATION_FAILED,            /**< Error when program failed to allocate memory - 4 */
    FUNCTION_ERROR,               /**< Generic function error @warning This should not occur! - 5 */
    FUNCTION_ARGUMENT_ERROR,      /**< Error when function gets unexpected value in argument - 6 */
    VALUE_ERROR,                  /**< Error when function gets bad value in argument - 7 */
    COMMAND_ERROR,                /**< Error when received invalid commands or invalid value - 8 */
    SELECTOR_ERROR,               /**< Error when invalid selector is received - 9 */
    NUM_CONVERSION_FAILED,        /**< Error when converting string to numeric value failed - 10 */
};

/**
 * @struct TempVariableStore
 * @brief Store for temporary variables
 */
typedef struct
{
    char **variables;   /**< Array of string corespoding to each temporary variable */
} TempVariableStore;

/**
 * @struct Raw_selector
 * @brief Stores raw data about table area selector
 */
typedef struct
{
    long long int lld_ir1;          /**< Numerical interpretation of r1 */
    long long int lld_ic1;          /**< Numerical interpretation of c1 */
    long long int lld_ir2;          /**< Numerical interpretation of r2 */
    long long int lld_ic2;          /**< Numerical interpretation of c2 */

    _Bool initialized;
} Raw_selector;

/**
 * @struct Raw_commands
 * @brief Store raw commands in form of string array
 */
typedef struct
{
    long long int num_of_commands; /**< Number of commands in array */
    char **commands; /**< Array of commands */
} Raw_commands;

/**
 * @struct Base_command
 * @brief Stores parsed commands in form of strings
 */
typedef struct
{
    char *function; /**< Function part of command */
    char *arguments; /**< Argument part of command */
} Base_command;

/**
 * @struct Base_commands
 * @brief Stores array of #Base_commands and info about their count
 */
typedef struct
{
    long long int num_of_commands; /**< Number of commands in array */
    Base_command *commands; /**< Array of commands */
} Base_commands;

/**
 * @struct Cell
 * @brief Store for data of single cell
 */
typedef struct
{
    long long int allocated_chars; /**< Size of allocated space in content */
    char *content; /**< Raw content of single cell */
} Cell;

/**
 * @struct Row
 * @brief Store for data of single row
 */
typedef struct
{
    long long int num_of_cells; /**< Number of cells in row */
    long long int allocated_cells; /**< Number of cell pointers allocated in memory */
    Cell *cells; /**< Pointer to first cell in row */
} Row;

/**
 * @struct Table
 * @brief Store for data of whole table
 */
typedef struct
{
    long long int num_of_rows; /**< Number of rows in table */
    long long int allocated_rows; /**< Number of row pointers allocated in memory */
    Row *rows; /**< Pointer to first row in table */
    char delim; /**< Delimiter for output */
} Table;

int trim_se(char *string)
{
    /**
     * @brief Trim start and end of string
     *
     * @param string String that we want to trim
     *
     * @return #NO_ERROR on success, #FUNCTION_ERROR on error
     */

    if (string == NULL)
        return FUNCTION_ERROR;

    unsigned long long int i = 1, len = strlen(string);
    if (len > 1)
    {
        for(; i < (len - 1); i++)
            string[i-1]=string[i];
    }
    string[i-1]='\0';

    return NO_ERROR;
}

int trim_start(char *string)
{
    /**
     * @brief Trim start and end of string
     *
     * @param string String that we want to trim
     *
     * @return #NO_ERROR on success, #FUNCTION_ERROR on error
     */

    if (string == NULL)
        return FUNCTION_ERROR;

    unsigned long long int i = 1, len = strlen(string);
    if (len > 1)
    {
        for(; i < len; i++)
            string[i-1]=string[i];
    }
    string[i-1]='\0';

    return NO_ERROR;
}

int string_copy(char **source, char **dest)
{
    /**
     * @brief Copy @p source string to @p dest string
     *
     * @param source Pointer to source string
     * @param dest Pointer to destination string
     *
     * @return #NO_ERROR on success, #ALLOCATION_FAILED on error
     */

    if (source == NULL)
        return NO_ERROR;

    *dest = (char*)malloc((strlen(*source) + 1) * sizeof(char));
    if (*dest == NULL)
        return ALLOCATION_FAILED;

    strcpy(*dest, *source);

    return NO_ERROR;
}

_Bool string_end_with(const char *base_string, const char *end_string)
{
    /**
     * @brief Check if string ends with other string
     *
     * Check if @p base_string ends with @p end_string
     *
     * @param base_string String where to look for substring
     * @param end_string Substring to look for at the end of base_string
     *
     * @return true if @p base_string ends with @p end_string, false if dont
     */

    char *end_start = strrchr(base_string, end_string[0]);
    if (end_start && strcmp(end_start, end_string) == 0)
        return true;
    return false;
}

_Bool string_start_with(const char *base_string, const char *start_string)
{
    /**
     * @brief Check if string starts with other string
     *
     * Check if @p base_string starts with @p start_string
     *
     * @param base_string String where to look for substring
     * @param start_string Substring to look for at start of base_string
     *
     * @return true if @p base_string starts with @p start_string, false if dont
     */

    if (strlen(start_string) > strlen(base_string))
        return false;

    return strncmp(start_string, base_string, strlen(start_string)) == 0;
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

    if (s1 == NULL && s2 == NULL)
        return true;

    if (s1 == NULL || s2 == NULL)
        return false;

    return strcmp(s1, s2) == 0;
}

_Bool is_string_llint(char *string)
{
    /**
     * @brief Check if @p string should be converted to long long int
     *
     * @param string String that we want to test
     *
     * @return true if it can be converted, false if not
     */

    if (string == NULL)
        return false;

    char *rest;
    strtoll(string, &rest, 10);
    if (strings_equal(rest, EMPTY_CELL))
        return true;
    return false;
}

_Bool is_string_ldouble(char *string)
{
    /**
     * @brief Check if @p string should be converted to long double
     *
     * @param string String that we want to test
     *
     * @return true if it can be converted, false if not
     */

    if (string == NULL)
        return false;

    char *rest;
    strtold(string, &rest);
    if (strings_equal(rest, EMPTY_CELL))
        return true;
    return false;
}

_Bool is_ldouble_lint(long double val)
{
    /**
     * @brief Check if long double could be converted to long int without loss of precision
     *
     * @param val Long double value to check
     *
     * @return true if can be converted, false if not
     */

    return (long int)(val) == val;
}

int string_to_llint(char *string, long long int *val)
{
    /**
     * @brief Convert string to long long int
     *
     * @param string String we want to convert
     * @param val Pointer to long long int where output will be saved
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (string == NULL)
        return FUNCTION_ARGUMENT_ERROR;

    char *rest;
    *val = strtoll(string, &rest, 10);

    if (!strings_equal(rest, EMPTY_CELL))
        return NUM_CONVERSION_FAILED;

    return NO_ERROR;
}

int string_to_ldouble(char *string, long double *val)
{
    /**
     * @brief Convert string to long double
     *
     * @param string String we want to convert
     * @param val Pointer to long double where output will be saved
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (string == NULL)
        return FUNCTION_ARGUMENT_ERROR;

    char *rest;
    *val = strtold(string, &rest);

    if (!strings_equal(rest, EMPTY_CELL))
        return NUM_CONVERSION_FAILED;

    return NO_ERROR;
}

int ldouble_to_string(long double value, char **output_string)
{
    /**
     * @brief Convert long double @p value to string
     *
     * @param value Long double value that we want to convert to string
     *
     * @return #NO_ERROR on success, #ALLOCATION_FAILED on error
     */

    char *temp_string = (char*)malloc(BASE_CELL_LENGTH * sizeof(char));
    if (temp_string == NULL)
        return ALLOCATION_FAILED;

    unsigned long long int tlength = snprintf(temp_string, BASE_CELL_LENGTH, "%Lg", value);
    if ((tlength + 1) > BASE_CELL_LENGTH)
    {
        char *tmp = (char*)realloc(temp_string, (tlength + 1) * sizeof(char));
        if (tmp == NULL)
        {
            free(temp_string);
            return ALLOCATION_FAILED;
        }

        temp_string = tmp;
        snprintf(temp_string, tlength + 1, "%Lg", value);
    }

    *output_string = temp_string;
    return NO_ERROR;
}

int lint_to_string(long int value, char **output_string)
{
    /**
     * @brief Convert long int @p value to string
     *
     * @param value Long int value that we want to convert to string
     *
     * @return #NO_ERROR on success, #ALLOCATION_FAILED on error
     */

    char *temp_string = (char*)malloc(BASE_CELL_LENGTH * sizeof(char));
    if (temp_string == NULL)
        return ALLOCATION_FAILED;

    unsigned long long int tlength = snprintf(temp_string, BASE_CELL_LENGTH, "%li", value);
    if ((tlength + 1) > BASE_CELL_LENGTH)
    {
        char *tmp = (char*)realloc(temp_string, (tlength + 1) * sizeof(char));
        if (tmp == NULL)
        {
            free(temp_string);
            return ALLOCATION_FAILED;
        }

        temp_string = tmp;
        snprintf(temp_string, tlength + 1, "%li", value);
    }

    *output_string = temp_string;
    return NO_ERROR;
}

long long int count_char(char *string, char c, _Bool ignore_escapes)
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
    _Bool in_double_parentecies = false;
    long long int counter = 0;

    for (size_t i = 0; i < lenght_of_string; i++)
    {
        char cc = string[i];

        if (!in_parentecies)
            if (cc == '"')
                in_double_parentecies = !in_double_parentecies;

        if (!in_double_parentecies)
            if (cc == '\'')
                in_parentecies = !in_parentecies;

        if (cc == c)
        {
            if (ignore_escapes || (!in_parentecies && !in_double_parentecies && (i != 0 && string[i-1] != '\\')))
            {
                counter++;
            }
        }
    }

    return counter;
}

long long int get_position_of_character(const char *string, char ch, long long int index, _Bool ignore_escapes)
{
    /**
     * @brief Get position of character of certain index in string
     *
     * Iterate over each character in string and count character ocurences
     * If ocurence counter coresponse to @p index then return position of current char in string
     *
     * @param string String where to find char
     * @param ch Character we are looking for
     * @param index Index of occurence of character in string we want position for
     * @param ignore_escapes Flag if we want ignore if characte is escaped or in parentecies and count it too
     *
     * @return Position of @p index ocurence of @p ch in string if valid ocurence is found, if not -1
     *
     * @todo Rework to strchr
     */

    long long int counter = 0;
    _Bool in_parentecies = false;
    _Bool in_double_parentecies = false;

    for (size_t i = 0; string[i]; i++)
    {
        if (!in_parentecies)
            if (string[i] == '"')
                in_double_parentecies = !in_double_parentecies;

        if (!in_double_parentecies)
            if (string[i] == '\'')
                in_parentecies = !in_parentecies;


        if (string[i] == ch)
        {
            if (ignore_escapes || (!in_parentecies && !in_double_parentecies && (i != 0 && string[i-1] != '\\')))
            {
                counter++;
                if ((counter - 1) == index)
                    return i;
            }
        }
    }

    return -1;
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

int get_substring(char *string, char **substring, char delim, long long int index, _Bool ignore_escapes, char **rest, _Bool want_rest)
{
    /**
     * @brief Get substring from string
     *
     * Extract Portion of string delimited by @p delim or by borders of main string @p string \n
     * Wantend portion is selected by @p index
     *
     * @param string Base string
     * @param substring Place where result will be saved, should be allocated large enough but if its not provided this function will allocate it
     * @param delim Deliminator character
     * @param index Index of substring
     * @param ignore_escapes Flag if we want ignore if characte is escaped or in parentecies and count it too
     * @param rest There will be saved rest of the string
     * @param want_rest Flag if we want save rest of the flag
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (string == NULL)
        return FUNCTION_ERROR;

    long long int number_of_delims = count_char(string, delim, ignore_escapes);
    size_t string_length = strlen(string);

    long long int start_index = (index == 0) ? 0 : get_position_of_character(string, delim, index - 1, ignore_escapes) + 1;
    long long int end_index = (index >= number_of_delims) ? ((long long int)string_length - 1) : get_position_of_character(string, delim, index, ignore_escapes) - 1;

    // If there is no substring pointer init it
    if ((*substring) == NULL)
    {
        if ((end_index - start_index + 1) == 0)
            return NO_ERROR;

        (*substring) = (char*)malloc((string_length + 1) * sizeof(char));
        if ((*substring) == NULL)
            return ALLOCATION_FAILED;
    }

    long long int i = 0, j = start_index;

    for (; j <= end_index; i++, j++)
    {
        (*substring)[i] = string[j];
    }
    (*substring)[end_index - start_index + 1] = '\0';

    if (want_rest)
    {
        j += 1;
        long long int rest_len = (long long int)string_length - j + 1;

        if (rest_len == 0)
            return NO_ERROR;

        (*rest) = (char*)malloc((rest_len) * sizeof(char));
        if (rest == NULL)
        {
            free(*substring);
            (*substring) = NULL;
            return ALLOCATION_FAILED;
        }

        i = 0;
        for (; j < (long long int)string_length; j++, i++)
        {
            (*rest)[i] = string[j];
        }
        (*rest)[i] = '\0';
    }

    return NO_ERROR;
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
        _Bool in_double_parentecies = false;

        for (size_t j = 0; j < length_of_line; j++)
        {
            char cc = line[j];

            if (!in_parentecies)
                if (cc == '"')
                    in_double_parentecies = !in_double_parentecies;

            if (!in_double_parentecies)
                if (cc == '\'')
                    in_parentecies = !in_parentecies;

            if (cc == delims[i])
            {
                if (!in_parentecies && !in_double_parentecies && (j != 0 && line[j-1] != '\\'))
                {
                    line[j] = delims[0];
                }
            }
        }
    }
}

void deallocate_cell(Cell *cell)
{
    /**
     * @brief Deallocate cell
     *
     * Free content of cell
     *
     * @param cell Pointer to instance of #Cell strucutre
     */

    if (cell->content == NULL)
        return;

    free(cell->content);
    cell->content = NULL;
    cell->allocated_chars = 0;
}

void deallocate_row(Row *row)
{
    /**
     * @brief Deallocate row
     *
     * Iterate over all cells and deallocate them
     *
     * @param row Pointer to instance of #Row structure
     */

    if (row->cells == NULL)
        return;

    for (long long int i = 0; i < row->num_of_cells; i++)
    {
        deallocate_cell(&row->cells[i]);
    }

    free(row->cells);
    row->cells = NULL;
    row->num_of_cells = 0;
    row->allocated_cells = 0;
}

void deallocate_table(Table *table)
{
    /**
     * @brief Dealocate whole table from memory
     *
     * Iterate over all rows and dealocate them
     *
     * @param table Pointer to instance of #Table structure
     */

    if (table->rows == NULL)
        return;

    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        deallocate_row(&table->rows[i]);
    }

    free(table->rows);
    table->rows = NULL;
    table->num_of_rows = 0;
    table->allocated_rows = 0;
}

void deallocate_raw_commands(Raw_commands *commands_store)
{
    /**
     * @brief Deallocate raw commands structure
     *
     * @param commands_store Pointer to instance of #Raw_commands structure
     */

    if (commands_store->commands == NULL)
        return;

    for (long long int i = 0; i < commands_store->num_of_commands; i++)
    {
        free(commands_store->commands[i]);
        commands_store->commands[i] = NULL;
    }

    free(commands_store->commands);
    commands_store->commands = NULL;
    commands_store->num_of_commands = 0;
}

void deallocate_base_commands(Base_commands *base_commands)
{
    /**
     * @brief Deallocate content of istance @p base_commands of #Base_commands structure
     *
     * @param base_commands Pointer to instance of #Base_commands structure
     */

    if (base_commands->commands == NULL)
        return;

    for (long long int i = 0; i < base_commands->num_of_commands; i++)
    {
        free(base_commands->commands[i].function);
        base_commands->commands[i].function = NULL;
        free(base_commands->commands[i].arguments);
        base_commands->commands[i].arguments = NULL;
    }

    free(base_commands->commands);
    base_commands->commands = NULL;
}

void deallocate_temp_var_store(TempVariableStore *temp_var_store)
{
    /**
     * @brief Deallocate temporary variable store
     *
     * @param temp_var_store Pointer to instance of #TempVariableStore structure
     */

    if (temp_var_store != NULL && temp_var_store->variables != NULL)
    {
        for (long long int i = 0; i < NUMBER_OF_TEMPORARY_VARIABLES; i++)
        {
            free(temp_var_store->variables[i]);
            temp_var_store->variables[i] = NULL;
        }

        free(temp_var_store->variables);
        temp_var_store->variables = NULL;
    }
}

int allocate_rows(Table *table)
{
    /**
     * @brief Allocate rows in table structure
     *
     * Allocate new rows array or extend existing one
     *
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR when table is allocated properly in other cases return #ALLOCATION_FAILED
     */

    if (table->rows == NULL)
    {
        table->rows = (Row*)malloc(BASE_NUMBER_OF_ROWS * sizeof(Row));
        if (table->rows == NULL)
            return ALLOCATION_FAILED;

        table->num_of_rows = 0;
        table->allocated_rows = BASE_NUMBER_OF_ROWS;
    }
    else
    {
        Row *tmp = (Row*)realloc(table->rows, (table->allocated_rows + BASE_NUMBER_OF_ROWS) * sizeof(Row));
        if (tmp == NULL)
            return ALLOCATION_FAILED;

        table->allocated_rows += BASE_NUMBER_OF_ROWS;
        table->rows = tmp;
    }

    // Set default values
    for (long long int i = table->num_of_rows; i < table->allocated_rows; i++)
    {
        table->rows[i].cells = NULL;
        table->rows[i].num_of_cells = 0;
        table->rows[i].allocated_cells = 0;
    }

    return NO_ERROR;
}

int allocate_raw_commands(Raw_commands *commands_store, long long int number_of_commands)
{
    /**
     * @brief Allocate commands store
     *
     * @param commands_store Pointer to instance of #Raw_commands structure
     * @param number_of_commands Number of commands we want to allocate
     *
     * @return #NO_ERROR on success, #ALLOCATION_FAILED on fail
     */

    commands_store->commands = (char**)malloc(number_of_commands * sizeof(char*));
    if (commands_store->commands == NULL)
        return ALLOCATION_FAILED;

    commands_store->num_of_commands = number_of_commands;
    return NO_ERROR;
}

int allocate_cells(Row *row)
{
    /**
     * @brief Allocate cells in row
     *
     * Allocate cell array in instance of #Row structure or extend existing one
     *
     * @param row Pointer to instance of #Row structure
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (row->cells == NULL)
    {
        row->cells = (Cell*)malloc(BASE_NUMBER_OF_CELLS * sizeof(Cell));
        if (row->cells == NULL)
            return ALLOCATION_FAILED;

        row->num_of_cells = 0;
        row->allocated_cells = BASE_NUMBER_OF_CELLS;
    }
    else
    {
        Cell *tmp = (Cell*)realloc(row->cells, (row->allocated_cells + BASE_NUMBER_OF_CELLS) * sizeof(Cell));
        if (tmp == NULL)
            return ALLOCATION_FAILED;

        row->allocated_cells += BASE_NUMBER_OF_CELLS;
        row->cells = tmp;
    }

    // Set default values
    for (long long int i = row->num_of_cells; i < row->allocated_cells; i++)
    {
        row->cells[i].content = NULL;
        row->cells[i].allocated_chars = 0;
    }

    return NO_ERROR;
}

int allocate_content(Cell *cell)
{
    /**
     * @brief Allocate content in cell
     *
     * Allocate content (char array) in instance of #Cell strucutre or extend existing one
     *
     * @param cell Pointer to instance of #Cell structure
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (cell->content == NULL)
    {
        cell->content = (char*)malloc(BASE_CELL_LENGTH * sizeof(char));
        if (cell->content == NULL)
            return ALLOCATION_FAILED;

        cell->allocated_chars = BASE_CELL_LENGTH * sizeof(char);
    }
    else
    {
        char *tmp = (char*)realloc(cell->content, (cell->allocated_chars + BASE_CELL_LENGTH) * sizeof(char));
        if (tmp == NULL)
            return ALLOCATION_FAILED;

        cell->allocated_chars += BASE_CELL_LENGTH;
        cell->content = tmp;
    }

    return NO_ERROR;
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

int get_commands(char *argv[], Raw_commands *commands_store, _Bool delim_flag_present)
{
    /**
     * @brief Get raw commands
     *
     * Get command string for aguments and open it as file and parse it to individual commands or parse the argument as individual commands
     *
     * @param argv Array of arguments
     * @param commands_store Pointer to instance of #Raw_commands structure where individual commands will be saved
     * @param delim_flag_present Flag if in argument is delim flag (only for calculating proper position of commands argument)
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    char *raw_commands = delim_flag_present ? argv[3] : argv[1];
    long long int num_of_commands = 0;

    if (string_start_with(raw_commands, "-c"))
    {
        // Raw commands are path to command file
        // Parse commands from commands file

        // Trim out prefix
        size_t len = strlen(raw_commands);
        memmove(raw_commands, raw_commands + 2, len - 1);

        // Try to open command file
        FILE *file = fopen(raw_commands, "r");
        if (file == NULL)
            return CANT_OPEN_FILE;

        char *line = NULL;

        // Count number of lines (commands because individual commands are on separated lines)
        while (get_line(&line, file) != -1)
        {
            free(line);
            line = NULL;
            num_of_commands++;
        }

        // Rewind to start of file
        rewind(file);

        if (allocate_raw_commands(commands_store, num_of_commands) != NO_ERROR)
            return ALLOCATION_FAILED;

        long long int i = 0;
        while (get_line(&line, file) != -1)
        {
            rm_newline_chars(line);
            commands_store->commands[i] = line;
            line = NULL;

            i++;
        }

        fclose(file);
    }
    else
    {
        // Parse commands from argument

        num_of_commands = count_char(raw_commands, ';', true) + 1;
        char *command = NULL;
        int ret_val;

        if (num_of_commands == 1 && strings_equal(raw_commands, EMPTY_CELL))
            return NO_ERROR;

        if (allocate_raw_commands(commands_store, num_of_commands) != NO_ERROR)
            return ALLOCATION_FAILED;

        for (long long int i = 0; i < num_of_commands; i++)
        {
            if ((ret_val = get_substring(raw_commands, &command, ';', i, true, NULL, false)) != NO_ERROR)
                return ret_val;

            commands_store->commands[i] = command;
            command = NULL;
        }
    }

    return NO_ERROR;
}

int parse_commands(Raw_commands *raw_command_store, Base_commands *base_command_store)
{
    /**
     * @brief Parse raw commands to base commands
     *
     * Split raw commands to function part and argument part
     *
     * @param raw_command_store Pointer to instance of #Raw_commands structure where raw commands are stored
     * @param base_command_store Pointer to instance #Base_commands structure where parsed commands will be stored
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (raw_command_store == NULL)
        return FUNCTION_ERROR;

    if (raw_command_store->num_of_commands != 0)
    {
        base_command_store->commands = (Base_command*)malloc(raw_command_store->num_of_commands * sizeof(Base_command));
        if (base_command_store->commands == NULL)
            return ALLOCATION_FAILED;

        base_command_store->num_of_commands = raw_command_store->num_of_commands;

        for (long long int i = 0; i < base_command_store->num_of_commands; i++)
        {
            base_command_store->commands[i].function = NULL;
            base_command_store->commands[i].arguments = NULL;
        }

        char *substring_buf = NULL;
        char *rest = NULL;
        int ret_val;

        for (long long int i = 0; i < raw_command_store->num_of_commands; i++)
        {
            // This is to not split find selector command
            if (string_start_with(raw_command_store->commands[i], "[") && string_end_with(raw_command_store->commands[i], "]"))
            {
                if ((ret_val = get_substring(raw_command_store->commands[i], &substring_buf, '\0', 0, false, &rest, true)) != NO_ERROR)
                    return ret_val;
            }
            else
            {
                if ((ret_val = get_substring(raw_command_store->commands[i], &substring_buf, ' ', 0, false, &rest, true)) != NO_ERROR)
                    return ret_val;
            }

            if (rest != NULL && strings_equal(rest, EMPTY_CELL))
            {
                free(rest);
                rest = NULL;
            }

            base_command_store->commands[i].function = substring_buf;
            base_command_store->commands[i].arguments = rest;

            substring_buf = NULL;
            rest = NULL;
        }
    }

    return NO_ERROR;
}

int parse_command_argument(char *command_argument, long long int **output_indexes, Table *table)
{
    /**
     * @brief Parse command arguments
     *
     * Split command arguments, convert them to numbers and return as array
     *
     * @param command_argument String with command arguments
     * @param output_indexes Pointer to output array of parsed arguments
     * @param table Pointer to instance of #Table structure for checking if values in argument are right
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;

    if (!string_start_with(command_argument, "[") || !string_end_with(command_argument, "]"))
        return COMMAND_ERROR;

    if ((ret_val = trim_se(command_argument)) != NO_ERROR)
        return ret_val;

    long long int num_of_parts = count_char(command_argument, ',', true) + 1;
    if (num_of_parts != 2)
        return COMMAND_ERROR;

    if (table->num_of_rows == 0 || table->rows[0].num_of_cells == 0)
        return FUNCTION_ERROR;

    char *parts[2] = { NULL };
    long long int *indexes = (long long int*)malloc(2 * sizeof(long long int));
    if (indexes == NULL)
        return ALLOCATION_FAILED;

    memset(indexes, 0, 2 * sizeof(long long int));

    for (long long int i = 0; i < num_of_parts; i++)
    {
        if ((ret_val = get_substring(command_argument, &parts[i], ',', i, true, NULL, false)) != NO_ERROR)
            break;

        if (is_string_llint(parts[i]))
        {
            if ((ret_val = string_to_llint(parts[i], &indexes[i])) != NO_ERROR)
                break;
        }
        else
        {
            if (strings_equal(parts[i], "-"))
                indexes[i] = (i == 0) ? table->num_of_rows : table->rows[0].num_of_cells;
            else
            {
                ret_val = COMMAND_ERROR;
                break;
            }
        }
    }

    for (int i = 0; i < num_of_parts; i++)
    {
        free(parts[i]);
        indexes[i] -= 1;

        if (ret_val == NO_ERROR)
            if ((indexes[i]) < 0 || indexes[i] >= (((i == 0) ? table->num_of_rows : table->rows[0].num_of_cells)))
                ret_val = COMMAND_ERROR;
    }

    (*output_indexes) = indexes;
    return ret_val;
}

void print_table(Table *table)
{
    /**
     * @brief Print table to console
     *
     * Debug functing for printing loaded table to console
     * @todo Remove
     *
     * @param table Pointer to instance of #Table structure
     */

    if (table->rows == NULL)
        return;

    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        if (table->rows[i].cells == NULL)
            return;

        for (long long int j = 0; j < table->rows[i].num_of_cells; j++)
        {
            if (table->rows[i].cells[j].content != NULL)
            {
                printf("%s", table->rows[i].cells[j].content);
                if (j < (table->rows[i].num_of_cells - 1))
                    printf("%c", table->delim);
            }
        }

        printf("\n");
    }
}

int save_table(Table *table, char *path)
{
    /**
     * @brief Save table to file
     *
     * Iterate over table and write to file its content
     *
     * @param table Pointer to instance of #Table structure
     * @param path Path to output file
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    // Try to open output file
    FILE *file = fopen(path, "w");
    if (file == NULL)
        return CANT_OPEN_FILE;

    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        // This only means that there is no data left
        if (table->rows[i].cells == NULL)
        {
            fclose(file);
            return NO_ERROR;
        }

        for (long long int j = 0; j < table->rows[i].num_of_cells; j++)
        {
            if (table->rows[i].cells[j].content != NULL)
            {
                fprintf(file, "%s", table->rows[i].cells[j].content);
                if (j < (table->rows[i].num_of_cells - 1))
                    fprintf(file, "%c", table->delim);
            }
        }

        fprintf(file, "\n");
    }

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

int set_cell(char *string, Cell *cell)
{
    /**
     * @brief Set value to cell content
     *
     * Try to allocate content in #Cell structure if content doesnt exist or rewrite existing data in it \n
     * If content is too small then extend it
     * @warning
     * If there are some data in cell it will be rewritten
     *
     * @param string String we want to set to cell
     * @param cell Pointer to instance of #Cell structure
     *
     * @return #NO_ERROR when setting value is successful or when allocation fails #ALLOCATION_FAILED
     */

    if (string == NULL)
        return FUNCTION_ARGUMENT_ERROR;

    if (cell->content == NULL)
    {
        // Allocate base cell
        if (allocate_content(cell) != NO_ERROR)
            return ALLOCATION_FAILED;
    }

    while ((strlen(string) + 1) > (unsigned long long)cell->allocated_chars)
        if (allocate_content(cell) != NO_ERROR)
            return ALLOCATION_FAILED;

    if (strcpy(cell->content, string) == NULL)
        return FUNCTION_ERROR;

    return NO_ERROR;
}

int set_value_in_area(Table *table, Raw_selector *selector, char *string)
{
    /**
     * @brief Set value of @p string in table area selected by @p selector in @p table
     *
     * @param table Pointer to instance of #Table structure that will be eddited
     * @param selector Pointer to instance of #Raw_selector structure for selecting are where we will set values
     * @param string String that we will set
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;
    if (table->num_of_rows == 0 || table->rows[0].num_of_cells == 0 || string == NULL)
        return COMMAND_ERROR;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if ((ret_val = set_cell(string, &table->rows[i].cells[j])) != NO_ERROR)
                return ret_val;
        }
    }

    return ret_val;
}

int swap_cells(Table *table, Raw_selector *selector, long long int r, long long int c)
{
    /**
     * @brief Swap cell with another one
     *
     * @warning
     * If more than one cell is selected then all selected cells will be swaped row by row and then cell by cell
     *
     * @param table Pointer to instance of #Table structure where output data will be saved
     * @param selector Pointer to instance of #Raw_selector structure that tell us where to find data
     * @param r Row index of base cell for swap
     * @param c Column index of base cell for swap
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (r < 0 || r > (table->num_of_rows - 1) || c < 0 || c > (table->rows[0].num_of_cells - 1))
        return FUNCTION_ARGUMENT_ERROR;

    int ret_val = NO_ERROR;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long int j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if ((i == r) && (j == c))
                continue;

            char *buff1 = NULL, *buff2 = NULL;

            if (((ret_val = string_copy(&table->rows[r].cells[c].content, &buff1)) == NO_ERROR) && ((ret_val = string_copy(&table->rows[i].cells[j].content, &buff2)) == NO_ERROR))
            {
                ret_val = set_cell(buff2, &table->rows[r].cells[c]);
                if (ret_val == NO_ERROR)
                    ret_val = set_cell(buff1, &table->rows[i].cells[j]);
            }

            free(buff1);
            free(buff2);

            if (ret_val != NO_ERROR)
                break;
        }

        if (ret_val != NO_ERROR)
            break;
    }

    return NO_ERROR;
}

int sum_cells(Table *table, Raw_selector *selector, long long int r, long long int c)
{
    /**
     * @brief Set sum of all numeric cells to output cell
     *
     * @warning
     * If non numeric cell is found then NaN will be outputed
     *
     * @param table Pointer to instance of #Table structure where output data will be saved
     * @param selector Pointer to instance of #Raw_selector structure that tell us where to find data
     * @param r Row index of output cell
     * @param c Column index of output cell
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (r < 0 || r > (table->num_of_rows - 1) || c < 0 || c > (table->rows[0].num_of_cells - 1))
        return FUNCTION_ARGUMENT_ERROR;

    int ret_val = NO_ERROR;

    long double sum = 0;
    _Bool nan = false;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long int j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if (is_string_ldouble(table->rows[i].cells[j].content))
            {
                long double tmp = 0;
                if ((ret_val = string_to_ldouble(table->rows[i].cells[j].content, &tmp)) != NO_ERROR)
                    return ret_val;

                sum += tmp;
            }
            else
            {
                nan = true;
                break;
            }
        }

        if (nan)
            break;
    }

    if (nan)
    {
        ret_val = set_cell("NaN", &table->rows[r].cells[c]);
    }
    else
    {
        char *temp_string = NULL;
        if ((ret_val = ldouble_to_string(sum, &temp_string)) == NO_ERROR)
        {
            ret_val = set_cell(temp_string, &table->rows[r].cells[c]);
            free(temp_string);
        }
    }

    return ret_val;
}

int avg_cells(Table *table, Raw_selector *selector, long long int r, long long int c)
{
    /**
     * @brief Set average value of numeric cells to output cell
     *
     * @warning
     * If non numeric cell is found then NaN will be outputed
     *
     * @param table Pointer to instance of #Table structure where output data will be saved
     * @param selector Pointer to instance of #Raw_selector structure that tell us where to find data
     * @param r Row index of output cell
     * @param c Column index of output cell
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (r < 0 || r > (table->num_of_rows - 1) || c < 0 || c > (table->rows[0].num_of_cells - 1))
        return FUNCTION_ARGUMENT_ERROR;

    int ret_val = NO_ERROR;

    long double sum = 0;
    long double num_of_vals = 0;
    _Bool nan = false;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long int j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if (is_string_ldouble(table->rows[i].cells[j].content))
            {
                long double tmp = 0;
                if ((ret_val = string_to_ldouble(table->rows[i].cells[j].content, &tmp)) != NO_ERROR)
                    return ret_val;

                sum += tmp;
                num_of_vals++;
            }
            else
            {
                nan = true;
                break;
            }
        }

        if (nan)
            break;
    }

    if (nan)
    {
        ret_val = set_cell("NaN", &table->rows[r].cells[c]);
    }
    else
    {
        sum = sum / num_of_vals;

        char *temp_string = NULL;
        if ((ret_val = ldouble_to_string(sum, &temp_string)) == NO_ERROR)
        {
            ret_val = set_cell(temp_string, &table->rows[r].cells[c]);
            free(temp_string);
        }
    }

    return ret_val;
}

int count_cells(Table *table, Raw_selector *selector, long long int r, long long int c)
{
    /**
     * @brief Set number of non empty cells to output cell
     *
     * @param table Pointer to instance of #Table structure where output data will be saved
     * @param selector Pointer to instance of #Raw_selector structure that tell us where to find data
     * @param r Row index of output cell
     * @param c Column index of output cell
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (r < 0 || r > (table->num_of_rows - 1) || c < 0 || c > (table->rows[0].num_of_cells - 1))
        return FUNCTION_ARGUMENT_ERROR;

    int ret_val = NO_ERROR;

    long double num_of_cells = 0;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long int j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if (!strings_equal(table->rows[i].cells[j].content, EMPTY_CELL))
                num_of_cells++;
        }
    }

    char *temp_string = NULL;
    if ((ret_val = ldouble_to_string(num_of_cells, &temp_string)) == NO_ERROR)
    {
        ret_val = set_cell(temp_string, &table->rows[r].cells[c]);
        free(temp_string);
    }

    return ret_val;
}

int cell_len(Table *table, Raw_selector *selector, long long int r, long long int c)
{
    /**
     * @brief Set length of string to output cell
     *
     * @warning
     * If more that one cell is selected then only length of last cell will be outputed
     *
     * @param table Pointer to instance of #Table structure where output data will be saved
     * @param selector Pointer to instance of #Raw_selector structure that tell us where to find data
     * @param r Row index of output cell
     * @param c Column index of output cell
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (r < 0 || r > (table->num_of_rows - 1) || c < 0 || c > (table->rows[0].num_of_cells - 1))
        return FUNCTION_ARGUMENT_ERROR;

    int ret_val = NO_ERROR;

    unsigned long int cell_length = (table->rows[selector->lld_ir2].cells[selector->lld_ic2].content != NULL) ? strlen(table->rows[selector->lld_ir2].cells[selector->lld_ic2].content) : 0;

    char *temp_string = NULL;
    if ((ret_val = ldouble_to_string((long double)cell_length, &temp_string)) == NO_ERROR)
    {
        ret_val = set_cell(temp_string, &table->rows[r].cells[c]);
        free(temp_string);
    }

    return ret_val;
}

int append_empty_cell(Row *row)
{
    /**
     * @brief Append empty cell to the end of the row
     *
     * @param row Pointer to instance of #Row structure where we want to append empty cell
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    // If row have no cells or its already full allocate new cols
    if (row->cells == NULL || row->num_of_cells == row->allocated_cells)
        if (allocate_cells(row) != NO_ERROR)
            return ALLOCATION_FAILED;

    if (set_cell(EMPTY_CELL, &row->cells[row->num_of_cells]) != NO_ERROR)
        return ALLOCATION_FAILED;

    row->num_of_cells++;

    return NO_ERROR;
}

int normalize_row_lengths(Table *table)
{
    /**
     * @brief Normalize lengths of rows
     *
     * Append empty cells to the end of smaller rows than the longest one to make all rows same length
     *
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR on success and #ALLOCATION_FAILED on error
     */

    long long int max_number_of_cols = 0;

    // Get maximum cols in whole table
    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        if (table->rows[i].num_of_cells > max_number_of_cols)
            max_number_of_cols = table->rows[i].num_of_cells;
    }

    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        long long int length_diff = max_number_of_cols - table->rows[i].num_of_cells;

        if (length_diff > 0)
        {
            for (long long int j = 0; j < length_diff; j++)
                if (append_empty_cell(&table->rows[i]) != NO_ERROR)
                    return ALLOCATION_FAILED;
        }
    }

    return NO_ERROR;
}

void normalize_empty_cols(Table *table)
{
    /**
     * @brief Trim excess colms
     *
     * Destroy empty cols on the end of rows
     *
     * @param table Pointer to instance of #Table structure
     */

    if (table->num_of_rows > 0)
    {
        for (long long int i = (table->rows[0].num_of_cells - 1); i > 0; i--)
        {
            _Bool all_empty = true;

            for (long long int j = 0; j < table->num_of_rows; j++)
            {
                if (!strings_equal(table->rows[j].cells[i].content, EMPTY_CELL))
                    all_empty = false;
            }

            if (all_empty)
            {
                // Destroy the empty ones
                for (long long int j = 0; j < table->num_of_rows; j++)
                {
                    deallocate_cell(&table->rows[j].cells[i]);
                    table->rows[j].num_of_cells--;
                }

                continue;
            }

            break;
        }
    }
}

int normalize_number_of_cols(Table *table)
{
    /**
     * @brief Normalize end of the rows
     *
     * Fill short rows to same size as the longest one and then trim all empty cols at the end
     *
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR on success and #ALLOCATION_FAILED on error
     */

    int ret_code;
    if ((ret_code = normalize_row_lengths(table) != NO_ERROR))
        return ret_code;

    normalize_empty_cols(table);

    return ret_code;
}

int create_row_from_data(char *line, Table *table)
{
    /**
     * @brief Create row from line data
     *
     * Parse and save line data
     *
     * @param line Input string to parse
     * @param table Pointer to instance #Table structure where row will be saved
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (line == NULL)
        return FUNCTION_ERROR;

    // Allocate base number of cells in row
    if (allocate_cells(&table->rows[table->num_of_rows]) != NO_ERROR)
        return ALLOCATION_FAILED;

    long long int number_of_cells = count_char(line, table->delim, false) + 1;

    char *substring_buffer = NULL;
    int ret_val;

    for (long long int i = 0; i < number_of_cells; i++)
    {
        if (i >= table->rows[table->num_of_rows].allocated_cells)
        {
            if (allocate_cells(&table->rows[table->num_of_rows]) != NO_ERROR)
                return ALLOCATION_FAILED;
        }

        if ((ret_val = get_substring(line, &substring_buffer, table->delim, i, false, NULL, false)) != NO_ERROR)
            return ret_val;

        if (set_cell(substring_buffer, &table->rows[table->num_of_rows].cells[i]) != NO_ERROR)
            return ALLOCATION_FAILED;

        table->rows[table->num_of_rows].num_of_cells++;
    }

    free(substring_buffer);

    table->num_of_rows++;
    return NO_ERROR;
}

void init_selector(Raw_selector *selector)
{
    /**
     * @brief Initialize @p selector to default values
     *
     * @param selector Pointer to instance of #Raw_selector structure
     */

    selector->lld_ic1 = 0;
    selector->lld_ic2 = 0;
    selector->lld_ir1 = 0;
    selector->lld_ir2 = 0;
    selector->initialized = true;
}

int init_temp_var_store(TempVariableStore *temp_var_store)
{
    /**
     * @brief Init array of temporary variables
     *
     * @param temp_var_store Pointer to instance of #TempVariableStore structure
     *
     * @return #NO_ERROR on success and #ALLOCATION_FAILED on error
     */

    temp_var_store->variables = (char**)malloc(NUMBER_OF_TEMPORARY_VARIABLES * sizeof(char*));
    if (temp_var_store->variables == NULL)
        return ALLOCATION_FAILED;

    for (long long int i = 0; i < NUMBER_OF_TEMPORARY_VARIABLES; i++)
    {
        temp_var_store->variables[i] = NULL;
    }

    return NO_ERROR;
}

void copy_selector(Raw_selector *source, Raw_selector *dest)
{
    /**
     * @brief Copy values from selector @p source to selector @p dest
     *
     * @param source Pointer to instance of #Raw_selector struct from which we want copy data
     * @param dest Pointer to instance of #Raw_selector struct where we want save data
     */

    dest->lld_ir1 = source->lld_ir1;
    dest->lld_ir2 = source->lld_ir2;
    dest->lld_ic1 = source->lld_ic1;
    dest->lld_ic2 = source->lld_ic2;
}

void selector_find(Raw_selector *selector, Table *table, char *string)
{
    /**
     * @brief Select cell that starts with @p string
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     * @param string String we want to find at the beginning of cell
     */

    _Bool found = false;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long int j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if (string_start_with(table->rows[i].cells[j].content, string))
            {
                selector->lld_ir1 = selector->lld_ir2 = i;
                selector->lld_ic1 = selector->lld_ic2 = j;
                found = true;
            }

            if (found)
                break;
        }

        if (found)
            break;
    }
}

int selector_max(Raw_selector *selector, Table *table)
{
    /**
     * @brief Select cell with maximum numeric value in current selection
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;
    long double max = -LDBL_MAX;
    long long int r = 0, c = 0;
    _Bool found = false;

    char *testing_string = NULL;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long int j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if (string_copy(&table->rows[i].cells[j].content, &testing_string) != NO_ERROR)
                return ALLOCATION_FAILED;

            if ((string_start_with(testing_string, "\"") && string_end_with(testing_string, "\"")) ||
                (string_start_with(testing_string, "\'") && string_end_with(testing_string, "\'")))
            {
                if (trim_se(testing_string) != NO_ERROR)
                {
                    free(testing_string);
                    return FUNCTION_ERROR;
                }
            }

            if (is_string_ldouble(testing_string))
            {
                long double ret;
                if ((ret_val = string_to_ldouble(testing_string, &ret)) != NO_ERROR)
                    break;

                if (ret > max)
                {
                    max = ret;
                    r = i;
                    c = j;
                    found = true;
                }
            }

            free(testing_string);
            testing_string = NULL;
        }
    }

    if (ret_val == NO_ERROR)
    {
        if (found)
        {
            selector->lld_ir1 = selector->lld_ir2 = r;
            selector->lld_ic1 = selector->lld_ic2 = c;
        }
        else
        {
            fprintf(stdout, "[WARNING] Cant find maximum in [%llu, %llu, %llu, %llu] selection\n", selector->lld_ir1 + 1, selector->lld_ic1 + 1, selector->lld_ir2 + 1, selector->lld_ic2 + 1);
        }
    }

    return ret_val;
}

int selector_min(Raw_selector *selector, Table *table)
{
    /**
     * @brief Select cell with minimum numeric value in current selection
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;
    long double min = LDBL_MAX;
    long long int r = 0, c = 0;
    _Bool found = false;

    char *testing_string = NULL;

    for (long long int i = selector->lld_ir1; (i <= selector->lld_ir2) && (i < table->num_of_rows); i++)
    {
        for (long long int j = selector->lld_ic1; (j <= selector->lld_ic2) && (j < table->rows[i].num_of_cells); j++)
        {
            if (string_copy(&table->rows[i].cells[j].content, &testing_string) != NO_ERROR)
                return ALLOCATION_FAILED;

            if ((string_start_with(testing_string, "\"") && string_end_with(testing_string, "\"")) ||
                (string_start_with(testing_string, "\'") && string_end_with(testing_string, "\'")))
            {
                if (trim_se(testing_string) != NO_ERROR)
                {
                    free(testing_string);
                    return FUNCTION_ERROR;
                }
            }

            if (is_string_ldouble(testing_string))
            {
                long double ret;
                if ((ret_val = string_to_ldouble(testing_string, &ret)) != NO_ERROR)
                    break;

                if (ret < min)
                {
                    min = ret;
                    r = i;
                    c = j;
                    found = true;
                }
            }

            free(testing_string);
            testing_string = NULL;
        }
    }

    if (ret_val == NO_ERROR)
    {
        if (found)
        {
            selector->lld_ir1 = selector->lld_ir2 = r;
            selector->lld_ic1 = selector->lld_ic2 = c;
        }
        else
        {
            fprintf(stdout, "[WARNING] Cant find minimum in [%llu, %llu, %llu, %llu] selection\n", selector->lld_ir1 + 1, selector->lld_ic1 + 1, selector->lld_ir2 + 1, selector->lld_ic2 + 1);
        }
    }

    return ret_val;
}

void selector_select_all(Raw_selector *selector, Table *table)
{
    /**
     * @brief Select whole table
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     */

    selector->lld_ir1 = selector->lld_ic1 = 0;
    selector->lld_ir2 = table->num_of_rows - 1;
    selector->lld_ic1 = 0;
    selector->lld_ic2 = table->rows[0].num_of_cells - 1;
}

void selector_select_last(Raw_selector *selector, Table *table)
{
    /**
     * @brief Select last cell in last row
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     */

    selector->lld_ir1 = selector->lld_ir2 = table->num_of_rows - 1;
    selector->lld_ic1 = selector->lld_ic2 = table->rows[0].num_of_cells - 1;
}

void selector_select_last_colm(Raw_selector *selector, Table *table)
{
    /**
     * @brief Select last column
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     */

    selector->lld_ir1 = 0;
    selector->lld_ir2 = table->num_of_rows - 1;
    selector->lld_ic1 = selector->lld_ic2 = table->rows[0].num_of_cells - 1;
}

void selector_select_last_row(Raw_selector *selector, Table *table)
{
    /**
     * @brief Select last row
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     */

    selector->lld_ir1 = selector->lld_ir2 = table->num_of_rows - 1;
    selector->lld_ic1 = 0;
    selector->lld_ic2 = table->rows[0].num_of_cells - 1;
}

int selector_select_4p_area(Raw_selector *selector, Table *table, char **parts, const _Bool *part_is_llint, const long long int *parts_llint)
{
    /**
     * @brief Parse and set data to selector for 4 parameter selectors
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     * @param parts Pointer to string array where splited data are stored
     * @param part_is_llint Pointer to array of bools that indicates if corespoding part is long long int
     * @param parts_llint Pointer to array of long long ints that are converted values from @p parts array if it was possible
     *
     * @return #NO_ERROR on success and #SELECTOR_ERROR if something failed
     */

    // Check if there are no _ characters in parts of selector
    if (!((!part_is_llint[2] && !strings_equal(parts[2], "-")) || (!part_is_llint[3] && !strings_equal(parts[3], "-"))) &&
        !((!part_is_llint[0] && !strings_equal(parts[0], "-")) || (!part_is_llint[1] && !strings_equal(parts[1], "-"))))
    {
        // Check if - chars are only on valid positions ([-,C1,R2,C2] doesnt make sense)
        if ((!part_is_llint[0] && part_is_llint[2]) || (!part_is_llint[1] && part_is_llint[3]) ||
            // Check if rows and cols values are valid (R1 <= R2, etc.)
            (part_is_llint[0] && part_is_llint[2] && parts_llint[0] > parts_llint[2]) ||
            (part_is_llint[1] && part_is_llint[3] && parts_llint[1] > parts_llint[3]) ||
            // Check ranges of numerical parts
            (part_is_llint[0] && (parts_llint[0] > table->num_of_rows || parts_llint[0] < 1)) || (part_is_llint[2] && (parts_llint[2] > table->num_of_rows || parts_llint[2] < 1)) ||
            (part_is_llint[1] && (parts_llint[1] > table->rows[0].num_of_cells || parts_llint[1] < 1)) || (part_is_llint[3] && (parts_llint[3] > table->rows[0].num_of_cells || parts_llint[3] < 1)))
        {
            return SELECTOR_ERROR;
        }

        selector->lld_ir1 = part_is_llint[0] ? parts_llint[0] - 1 : table->num_of_rows - 1;
        selector->lld_ic1 = part_is_llint[1] ? parts_llint[1] - 1 : table->rows[0].num_of_cells - 1;
        selector->lld_ir2 = part_is_llint[2] ? parts_llint[2] - 1 : table->num_of_rows - 1;
        selector->lld_ic2 = part_is_llint[3] ? parts_llint[3] - 1 : table->rows[0].num_of_cells - 1;

        return NO_ERROR;
    }

    return SELECTOR_ERROR;
}

int selector_select_2p_area(Raw_selector *selector, Table *table, char **parts, const _Bool *part_is_llint, const long long int *parts_llint)
{
    /**
     * @brief Parse and set data to selector for 2 parameter selectors
     *
     * @param selector Pointer to instance of #Raw_selector structure where data will be saved
     * @param table Pointer to instance of #Table structure
     * @param parts Pointer to string array where splited data are stored
     * @param part_is_llint Pointer to array of bools that indicates if corespoding part is long long int
     * @param parts_llint Pointer to array of long long ints that are converted values from @p parts array if it was possible
     *
     * @return #NO_ERROR on success and #SELECTOR_ERROR if something failed
     */

    if (part_is_llint[0] && part_is_llint[1])
    {
        // [R,C]
        if (parts_llint[0] > 0 && parts_llint[0] <= table->num_of_rows && parts_llint[1] > 0 && parts_llint[1] <= table->rows[0].num_of_cells)
        {
            selector->lld_ir1 = selector->lld_ir2 = parts_llint[0] - 1;
            selector->lld_ic1 = selector->lld_ic2 = parts_llint[1] - 1;
            return NO_ERROR;
        }
    }
    else if (part_is_llint[0] && !part_is_llint[1])
    {
        // [R,_]
        if (parts_llint[0] > 0 && parts_llint[0] <= table->num_of_rows && strings_equal(parts[1], "_"))
        {
            selector->lld_ir1 = selector->lld_ir2 = parts_llint[0] - 1;
            selector->lld_ic1 = 0;
            selector->lld_ic2 = table->rows[0].num_of_cells - 1;
            return NO_ERROR;
        }
            // [R,-]
        else if (parts_llint[0] > 0 && parts_llint[0] <= table->num_of_rows && strings_equal(parts[1], "-"))
        {
            selector->lld_ir1 = selector->lld_ir2 = parts_llint[0] - 1;
            selector->lld_ic1 = selector->lld_ic2 = table->rows[0].num_of_cells - 1;
            return NO_ERROR;
        }
    }
    else if (!part_is_llint[0] && part_is_llint[1])
    {
        // [_,C]
        if (strings_equal(parts[0], "_") && parts_llint[1] > 0 && parts_llint[1] <= table->rows[0].num_of_cells)
        {
            selector->lld_ir1 = 0;
            selector->lld_ir2 = table->num_of_rows - 1;
            selector->lld_ic1 = selector->lld_ic2 = parts_llint[1] - 1;
            return NO_ERROR;
        }
            // [-,C]
        else if (strings_equal(parts[0], "-") && parts_llint[1] > 0 && parts_llint[1] <= table->rows[0].num_of_cells)
        {
            selector->lld_ir1 = selector->lld_ir2 = table->num_of_rows - 1;
            selector->lld_ic1 = selector->lld_ic2 = parts_llint[1] - 1;
            return NO_ERROR;
        }
    }

    return SELECTOR_ERROR;
}

int set_selector(Raw_selector *selector, Raw_selector *temp_selector, Base_command *command, Table *table) {
    /**
     * @brief Set values in @p selector
     *
     * Set values in selector based on inputed @p command and @p table
     *
     * @param selector Pointer to instance of #Raw_selector structure (main selector)
     * @param temp_selector Pointer to instance of #Raw_selector structure (temporary selector)
     * @param command Pointer to instance of #Base_command structure
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val;

    if (!selector->initialized || !temp_selector->initialized)
        return FUNCTION_ERROR;

    // Get rid of []
    if ((ret_val = trim_se(command->function)) != NO_ERROR)
        return ret_val;

    char *buffer = NULL;
    char *rest_buf = NULL;
    if ((ret_val = get_substring(command->function, &buffer, ' ', 0, true, &rest_buf, true)) != NO_ERROR)
        return ret_val;

    if (strings_equal(buffer, "find"))
    {
        selector_find(selector, table, rest_buf);
    }
    else
    {
        // Static selections
        if (strings_equal(buffer, "max"))
        {
            ret_val = selector_max(selector, table);
        }
        else if (strings_equal(buffer, "min"))
        {
            ret_val = selector_min(selector, table);
        }
        else if (strings_equal(buffer, "_,_"))
        {
            selector_select_all(selector, table);
        }
        else if (strings_equal(buffer, "-,-") || strings_equal(buffer, "-,-,-,-"))
        {
            selector_select_last(selector, table);
        }
        else if (strings_equal(buffer, "_,-"))
        {
            selector_select_last_colm(selector, table);
        }
        else if (strings_equal(buffer, "-,_"))
        {
            selector_select_last_row(selector, table);
        }
        else if (strings_equal(buffer, "_"))
        {
            copy_selector(temp_selector, selector);
        }
        else if (strings_equal(buffer, "set"))
        {
            copy_selector(selector, temp_selector);
        }
        else
        {
            long long int num_of_parts = count_char(buffer, ',', true) + 1;
            char *parts[4] = { NULL };
            _Bool part_is_llint[4] = { false };
            long long int parts_llint[4] = { 0 };

            // Parse more complex selections
            for (long long int i = 0; i < num_of_parts; i++)
            {
                if ((ret_val = get_substring(buffer, &parts[i], ',', i, true, NULL, false)) != NO_ERROR)
                    break;

                if ((part_is_llint[i] = is_string_llint(parts[i])) == true)
                {
                    if ((ret_val = string_to_llint(parts[i], &parts_llint[i])) != NO_ERROR)
                        break;
                }
                else
                {
                    if (!strings_equal(parts[i], "-") && !strings_equal(parts[i], "_"))
                    {
                        ret_val = SELECTOR_ERROR;
                        break;
                    }
                }
            }

            // If there is no error continue to validity check and setting new selector
            if (ret_val == NO_ERROR)
            {
                switch (num_of_parts)
                {
                    case 2:
                        // Parser for 2 parts selectors
                        ret_val = selector_select_2p_area(selector, table, parts, part_is_llint, parts_llint);
                        break;

                    case 4:
                        // Parser for 4 parts selectors like [R1,C1,R2,C2], [R1,C1,R2,-], etc
                        ret_val = selector_select_4p_area(selector, table, parts, part_is_llint, parts_llint);
                        break;

                    default:
                        ret_val = SELECTOR_ERROR;
                        break;
                }
            }

            for (int j = 0; j < 4; j++)
                free(parts[j]);
        }
    }

    free(buffer);
    free(rest_buf);

    return ret_val;
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
     * @param table Pointer #Table where data will be saved
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;
    FILE *file;
    char *line = NULL;
    long long int line_index = 0;

    // Try to open input file
    file = fopen(filepath, "r");
    if (file == NULL)
        return CANT_OPEN_FILE;

    // Allocate first row
    if (allocate_rows(table) != NO_ERROR)
    {
        fclose(file);
        return ALLOCATION_FAILED;
    }

    // Iterate thru input file
    // When we set buffer size to 0 and output char pointer to NULL then getline will allocate memory itself
    while (get_line(&line, file) != -1)
    {
        if (line == NULL)
        {
            ret_val = ALLOCATION_FAILED;
            break;
        }

        rm_newline_chars(line);

        normalize_delims(line, delims);

        // Check if we still have room in rows array
        if (line_index >= table->allocated_rows)
            // Allocate larger array of rows
            if (allocate_rows(table) != NO_ERROR)
            {
                ret_val = ALLOCATION_FAILED;
                break;
            }

        if ((ret_val = create_row_from_data(line, table)) != NO_ERROR)
            break;

        free(line);
        line_index++;
    }


    // Close input file
    fclose(file);

    return ret_val;
}

int delete_col(Table *table, long long int index)
{
    /**
     * @brief Delete column
     *
     * Delete content of cells in colm of @p index and shift all cols on the right side of it leftside
     *
     * @param table Pointer to instance of #Table structure
     * @param index Index of column we want to delete
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (table->rows == NULL || table->num_of_rows == 0)
        return VALUE_ERROR;


    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        if (table->rows[i].num_of_cells <= index || index < 0)
            return FUNCTION_ARGUMENT_ERROR;

        long long int j = index;
        if ((table->rows[i].num_of_cells - 1) > index)
        {
            for (j = index + 1; j < table->rows[i].num_of_cells; j++)
            {
                if (table->rows[i].cells[j].content == NULL)
                    return FUNCTION_ERROR;

                if (set_cell(table->rows[i].cells[j].content, &table->rows[i].cells[j - 1]) != NO_ERROR)
                    return ALLOCATION_FAILED;
            }
            j--;
        }

        deallocate_cell(&table->rows[i].cells[j]);
        table->rows[i].num_of_cells--;
    }

    return NO_ERROR;
}

int delete_cols(Table *table, long long int start_index, long long int end_index)
{
    /**
     * @brief Delete columns
     *
     * Delete content of cells in colms within range of @p start_index and @p end_index
     *
     * @param table Pointer to instance of #Table structure
     * @param start_index Start index of column we want to delete
     * @param end_index End index of column we want to delete
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;

    if (end_index >= table->rows[0].num_of_cells)
        end_index = table->rows[0].num_of_cells - 1;

    for (long long int i = end_index; i >= start_index; i--)
    {
        if ((ret_val = delete_col(table, i)) != NO_ERROR)
            break;
    }

    return ret_val;
}

int append_col(Table *table)
{
    /**
     * @brief Append empty column to right of the table
     *
     * Iterate over all rows and add empty column to the end of each of them
     *
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (table->rows == NULL || table->num_of_rows == 0)
        return VALUE_ERROR;

    int ret_val;

    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        if ((ret_val = append_empty_cell(&table->rows[i])) != NO_ERROR)
            return ret_val;
    }

    return NO_ERROR;
}

int insert_col(Table *table, long long int index)
{
    /**
     * @brief Insert column to table
     *
     * Insert empty column to table in the @p index and push all columns to the right
     *
     * @param table Pointer to instance of #Table struct
     * @param index Index of column where we want insert new one
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (table->rows == NULL || table->num_of_rows == 0)
        return VALUE_ERROR;

    for (long long int i = 0; i < table->num_of_rows; i++)
    {
        if (table->rows[i].num_of_cells <= index || index < 0)
            return FUNCTION_ARGUMENT_ERROR;

        if (table->rows[i].cells == NULL)
            return FUNCTION_ERROR;

        if (table->rows[i].num_of_cells == table->rows[i].allocated_cells)
        {
            // Allocate more space
            if (allocate_cells(&table->rows[i]) != NO_ERROR)
                return ALLOCATION_FAILED;
        }

        for (long long j = table->rows[i].num_of_cells; j > index; j--)
        {
            if (table->rows[i].cells[j - 1].content == NULL)
                return FUNCTION_ERROR;

            if (set_cell(table->rows[i].cells[j - 1].content, &table->rows[i].cells[j]) != NO_ERROR)
                return ALLOCATION_FAILED;
        }

        if (set_cell(EMPTY_CELL, &table->rows[i].cells[index]) != NO_ERROR)
            return ALLOCATION_FAILED;

        table->rows[i].num_of_cells++;
    }

    return NO_ERROR;
}

int append_row(Table *table)
{
    /**
     * @brief Append row to the bottom of table
     *
     * @param table Pointer to instance of #Table structure
     *
     * @return #NO_ERROR on success, #ALLOCATION_FAILED on error
     */

    // if there is no reference row then create new row with one cell
    long long int number_of_cells = table->num_of_rows > 0 ? table->rows[0].num_of_cells : 1;

    // If there is no more space
    if (table->num_of_rows == table->allocated_rows)
    {
        // Allocate more rows
        if (allocate_rows(table) != NO_ERROR)
            return ALLOCATION_FAILED;
    }

    for (long long int i = 0; i < number_of_cells; i++)
    {
        if (append_empty_cell(&table->rows[table->num_of_rows]) != NO_ERROR)
            return ALLOCATION_FAILED;
    }

    table->num_of_rows++;

    return NO_ERROR;
}

int insert_row(Table *table, long long int index)
{
    /**
     * @brief Insert row to table
     *
     * Insert empty row to the table on place of @p index and shift all rows down
     *
     * @param table Pointer to instance of #Table struct
     * @param index Index of row where we want to place new row
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (table->rows == NULL || table->num_of_rows == 0)
        return VALUE_ERROR;

    if (table->num_of_rows <= index || index < 0)
        return FUNCTION_ARGUMENT_ERROR;

    // if there is no reference row then create new row with one cell
    long long int number_of_cells = table->num_of_rows > 0 ? table->rows[0].num_of_cells : 1;

    // If there is no more space
    if (table->num_of_rows == table->allocated_rows)
    {
        // Allocate more rows
        if (allocate_rows(table) != NO_ERROR)
            return ALLOCATION_FAILED;
    }

    for (long long int i = table->num_of_rows; i > index; i--)
    {
        if (table->rows[i - 1].cells == NULL)
            return FUNCTION_ERROR;

        table->rows[i].cells = table->rows[i - 1].cells;
        table->rows[i].num_of_cells = table->rows[i - 1].num_of_cells;
        table->rows[i].allocated_cells = table->rows[i - 1].allocated_cells;
    }

    // Clear what is in original index
    table->rows[index].cells = NULL;
    table->rows[index].num_of_cells = 0;
    table->rows[index].allocated_cells = 0;

    for (long long int i = 0; i < number_of_cells; i++)
        if (append_empty_cell(&table->rows[index]) != NO_ERROR)
            return ALLOCATION_FAILED;

    table->num_of_rows++;

    return NO_ERROR;
}

int delete_row(Table *table, long long int index)
{
    /**
     * @brief Delete row
     *
     * Delete content of row on @p index and shift all rows bellow it up
     *
     * @param table Pointer to instance of #Table structure
     * @param index Index of row we want to delete
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    if (table->rows == NULL || table->num_of_rows == 0)
        return VALUE_ERROR;

    if (index < 0 || index >= table->num_of_rows)
        return FUNCTION_ARGUMENT_ERROR;

    deallocate_row(&table->rows[index]);

    if (index < (table->num_of_rows - 1))
    {
        for (long long int i = (index + 1); i < table->num_of_rows; i++)
        {
            if (table->rows[i].cells == NULL)
                return FUNCTION_ERROR;

            table->rows[i - 1].num_of_cells = table->rows[i].num_of_cells;
            table->rows[i - 1].allocated_cells = table->rows[i].allocated_cells;
            table->rows[i - 1].cells = table->rows[i].cells;
        }
    }

    table->num_of_rows--;

    return NO_ERROR;
}

int delete_rows(Table *table, long long int start_index, long long int end_index)
{
    /**
     * @brief Delete rows
     *
     * Delete content of rows and cells in them in rows within range of @p start_index and @p end_index
     *
     * @param table Pointer to instance of #Table structure
     * @param start_index Start index of column we want to delete
     * @param end_index End index of column we want to delete
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;

    if (end_index >= table->num_of_rows)
        end_index = table->num_of_rows - 1;

    for (long long int i = end_index; i >= start_index; i--)
    {
        if ((ret_val = delete_row(table, i)) != NO_ERROR)
            return ret_val;
    }

    return ret_val;
}

int set_temporary_variable(Table *table, Raw_selector *selector, TempVariableStore *temp_var_store, long long int index)
{
    /**
     * @brief Set temporary variable
     *
     * Allocate and set value in temporary variable of @p index
     *
     * @param table Pointer to instance of #Table structure that will be eddited
     * @param selector Pointer to instance of #Raw_selector structure for selecting value we want to save
     * @param temp_var_store Pointer to instance of #TempVariableStore structure used for stroring values
     * @param index Index of temporary variable we want to work with
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;

    if (table->num_of_rows > 0 && table->rows[0].num_of_cells > 0 &&
        table->num_of_rows > selector->lld_ir1 && table->rows[0].num_of_cells > selector->lld_ic1 &&
        table->rows[selector->lld_ir1].cells[selector->lld_ic1].content != NULL)
    {
        if (temp_var_store->variables[index] != NULL)
        {
            free(temp_var_store->variables[index]);
            temp_var_store->variables[index] = NULL;
        }

        char *temp_string = NULL;
        if ((ret_val = string_copy(&table->rows[selector->lld_ir1].cells[selector->lld_ic1].content, &temp_string)) != NO_ERROR)
            return ret_val;

        temp_var_store->variables[index] = temp_string;
    }

    return ret_val;
}

int set_cell_from_temporary_variable(Table *table, Raw_selector *selector, TempVariableStore *temp_var_store, long long int index)
{
    /**
     * @brief Set value from temporary variable to cell
     *
     * Set temporary variable of @p index to cell selected by @p selector
     *
     * @param table Pointer to instance of #Table structure that will be eddited
     * @param selector Pointer to instance of #Raw_selector structure for selecting value we want to use
     * @param temp_var_store Pointer to instance of #TempVariableStore structure used for stroring values
     * @param index Index of temporary variable we want to work with
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;

    if (table->num_of_rows > 0 && table->rows[0].num_of_cells > 0 &&
        temp_var_store->variables[index] != NULL)
    {
        ret_val = set_value_in_area(table, selector, temp_var_store->variables[index]);
    }

    return ret_val;
}

int increase_temporary_variable(TempVariableStore *temp_var_store, long long int index)
{
    /**
     * @brief Increase value in temporary variable
     *
     * Increase value of temporary variable if exist\n
     * If value is no numeric value then set its value to 1
     *
     * @param temp_var_store Pointer to instance of #TempVariableStore structure used for stroring values
     * @param index Index of temporary variable we want to work with
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;

    if (temp_var_store->variables[index] != NULL)
    {
        // Create temp variables
        long double temp_val = 0;

        if (is_string_ldouble(temp_var_store->variables[index]))
        {
            if ((ret_val = string_to_ldouble(temp_var_store->variables[index], &temp_val)) != NO_ERROR)
                return ret_val;

            temp_val += 1.0;
        }
        else
            temp_val = 1.0;

        char *temp_string = NULL;

        // Format string value
        if (is_ldouble_lint(temp_val))
        {
            if ((ret_val = lint_to_string((long int)temp_val, &temp_string)) != NO_ERROR)
                return ret_val;
        }
        else
        {
            if ((ret_val = ldouble_to_string(temp_val, &temp_string)) != NO_ERROR)
                return ret_val;
        }

        // Clear old variable
        free(temp_var_store->variables[index]);

        // Copy new string to variable
        if ((ret_val = string_copy(&temp_string, &temp_var_store->variables[index])) != NO_ERROR)
        {
            free(temp_string);
            return ret_val;
        }

        // Clear temporary string
        free(temp_string);
    }
    else
    {
        temp_var_store->variables[index] = (char*)malloc(2 * sizeof(char));
        if (temp_var_store->variables[index] == NULL)
            return ALLOCATION_FAILED;

        if (strcpy(temp_var_store->variables[index], "1") == NULL)
            return FUNCTION_ERROR;
    }

    return ret_val;
}

_Bool is_command_selector(Base_command *command)
{
    /**
     * @brief Check if command is selector
     *
     * @param command Pointer to instance of #Base_command structure
     *
     * @return true if it is selector command and false if not
     */

    if (command->arguments == NULL && string_start_with(command->function, "[") && string_end_with(command->function, "]"))
        return true;
    return false;
}

_Bool is_table_editing_command(Base_command *command)
{
    /**
     * @brief Check if inputed command is table editing command
     *
     * @param command Pointer to instance of #Base_command structure
     *
     * @return true if its table editing command, false if not
     */

    for (int i = 0; i < NUMBER_OF_TABLE_EDITING_COMMANDS; i++)
    {
        if (strings_equal(command->function, TABLE_EDITING_COMMANDS[i]))
            return true;
    }

    return false;
}

_Bool is_data_editing_command(Base_command *command)
{
    /**
     * @brief Check if inputed command is table data command
     *
     * @param command Pointer to instance of #Base_command structure
     *
     * @return true if its data editing command, false if not
     */

    for (int i = 0; i < NUMBER_OF_DATA_EDITING_COMMANDS; i++)
    {
        if (strings_equal(command->function, DATA_EDITING_COMMANDS[i]))
            return true;
    }

    return false;
}

_Bool is_temp_var_command(Base_command *command)
{
    /**
     * @brief Check if inputed command is temporary variable command
     *
     * @param command Pointer to instance of #Base_command structure
     *
     * @return true if its temporary variable command, false if not
     */

    for (int i = 0; i < NUMBER_OF_TEMP_VAR_COMMANDS; i++)
    {
        if (strings_equal(command->function, TEMP_VAR_COMMANDS[i]))
            return true;
    }

    return false;
}

int get_table_editing_command_index(Base_command *command)
{
    /**
     * @brief Get index of table editing command from reference array
     *
     * @param command Pointer to instance of #Base_command structure
     *
     * @return Index of command from reference #TABLE_EDITING_COMMANDS array if command is found, in other cases return -1
     */

    if (!is_table_editing_command(command))
        return -1;

    for (int i = 0; i < NUMBER_OF_TABLE_EDITING_COMMANDS; i++)
    {
        if (strings_equal(command->function, TABLE_EDITING_COMMANDS[i]))
            return i;
    }

    return -1;
}

int get_data_editing_command_index(Base_command *command)
{
    /**
     * @brief Get index of data editing command from reference array
     *
     * @param command Pointer to instance of #Base_command structure
     *
     * @return Index of command from reference #DATA_EDITING_COMMANDS array if command is found, in other cases return -1
     */

    if (!is_data_editing_command(command))
        return -1;

    for (int i = 0; i < NUMBER_OF_DATA_EDITING_COMMANDS; i++)
    {
        if (strings_equal(command->function, DATA_EDITING_COMMANDS[i]))
            return i;
    }

    return -1;
}

int get_temp_var_command_index(Base_command *command)
{
    /**
     * @brief Get index of temporary variable command from reference array
     *
     * @param command Pointer to instance of #Base_command structure
     *
     * @return Index of command from reference #TEMP_VAR_COMMANDS array if command is found, in other cases return -1
     */

    if (!is_temp_var_command(command))
        return -1;

    for (int i = 0; i < NUMBER_OF_TEMP_VAR_COMMANDS; i++)
    {
        if (strings_equal(command->function, TEMP_VAR_COMMANDS[i]))
            return i;
    }

    return -1;
}

int execute_table_editing_comm(Table *table, Raw_selector *selector, Base_command *command)
{
    /**
     * @brief Execute table editing command on @p table
     *
     * Assign proper function to @p command and execute it to edit table
     *
     * @param table Pointer to instance of #Table structure that will be eddited
     * @param selector Pointer to instance of #Raw_selector structure for selecting part of @p table to edit
     * @param command Pointer to instance of #Base_command structure that will select function to use
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;
    int findex = get_table_editing_command_index(command);
    if (findex == -1)
        return NO_ERROR;

    switch (findex)
    {
        // irow
        case 0:
            ret_val = insert_row(table, selector->lld_ir1);
            break;

            // arow
        case 1:
            if (selector->lld_ir2 >= table->num_of_rows - 1)
                ret_val = append_row(table);
            else
                ret_val = insert_row(table, selector->lld_ir2 + 1);
            break;

            // drow
        case 2:
            ret_val = delete_rows(table, selector->lld_ir1, selector->lld_ir2);
            break;

            // icol
        case 3:
            if (table->num_of_rows > 0)
                ret_val = insert_col(table, selector->lld_ic1);
            break;

            // acol
        case 4:
            if (table->num_of_rows > 0)
            {
                if (selector->lld_ic2 >= table->rows[0].num_of_cells - 1)
                    ret_val = append_col(table);
                else
                    ret_val = insert_col(table, selector->lld_ic2 + 1);
            }
            break;

            // dcol
        case 5:
            if (table->num_of_rows > 0)
                ret_val = delete_cols(table, selector->lld_ic1, selector->lld_ic2);
            break;

        default:
            ret_val = COMMAND_ERROR;
            break;
    }

    return ret_val;
}

int execute_data_editing_command(Table *table, Raw_selector *selector, Base_command *command)
{
    /**
     * @brief Execute data editing command on @p table
     *
     * Assign proper function to @p command and execute it to edit table
     *
     * @param table Pointer to instance of #Table structure that will be eddited
     * @param selector Pointer to instance of #Raw_selector structure for selecting part of @p table to edit
     * @param command Pointer to instance of #Base_command structure that will select function to use
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;
    int findex = get_data_editing_command_index(command);
    if (findex == -1)
        return NO_ERROR;

    long long int *advanced_args = NULL;

    switch (findex)
    {
        // set STR
        case 0:
            ret_val = set_value_in_area(table, selector, command->arguments);
            break;

        // clear
        case 1:
            ret_val = set_value_in_area(table, selector, EMPTY_CELL);
            break;

        // swap [R,C]
        case 2:
            if ((ret_val = parse_command_argument(command->arguments, &advanced_args, table)) == NO_ERROR)
            {
                if (advanced_args != NULL)
                    ret_val = swap_cells(table, selector, advanced_args[0], advanced_args[1]);
                else
                    ret_val = FUNCTION_ERROR;
            }
            break;

        // sum [R,C]
        case 3:
            if ((ret_val = parse_command_argument(command->arguments, &advanced_args, table)) == NO_ERROR)
            {
                if (advanced_args != NULL)
                    ret_val = sum_cells(table, selector, advanced_args[0], advanced_args[1]);
                else
                    ret_val = FUNCTION_ERROR;
            }
            break;

        // avg [R,C]
        case 4:
            if ((ret_val = parse_command_argument(command->arguments, &advanced_args, table)) == NO_ERROR)
            {
                if (advanced_args != NULL)
                    ret_val = avg_cells(table, selector, advanced_args[0], advanced_args[1]);
                else
                    ret_val = FUNCTION_ERROR;
            }
            break;

        // count [R,C]
        case 5:
            if ((ret_val = parse_command_argument(command->arguments, &advanced_args, table)) == NO_ERROR)
            {
                if (advanced_args != NULL)
                    ret_val = count_cells(table, selector, advanced_args[0], advanced_args[1]);
                else
                    ret_val = FUNCTION_ERROR;
            }
            break;

        // len [R,C]
        case 6:
            if ((ret_val = parse_command_argument(command->arguments, &advanced_args, table)) == NO_ERROR)
            {
                if (advanced_args != NULL)
                    ret_val = cell_len(table, selector, advanced_args[0], advanced_args[1]);
                else
                    ret_val = FUNCTION_ERROR;
            }
            break;

        default:
            ret_val = COMMAND_ERROR;
            break;
    }

    free(advanced_args);
    return ret_val;
}

int execute_temp_var_command(Table *table, Raw_selector *selector, Base_command *command, TempVariableStore *temp_var_store)
{
    /**
     * @brief Execute temporary variable command on @p table
     *
     * Assign proper function to @p command and execute it to edit table
     *
     * @param table Pointer to instance of #Table structure that will be eddited
     * @param selector Pointer to instance of #Raw_selector structure for selecting part of @p table to edit
     * @param command Pointer to instance of #Base_command structure that will select function to use
     * @param temp_var_store Pointer to instance of #TempVariableStore structure used for stroring values
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;
    int findex = get_temp_var_command_index(command);
    if (findex == -1)
        return NO_ERROR;

    char *arg = NULL;
    long long int arg_lli = -1;

    if ((ret_val = string_copy(&command->arguments, &arg)) != NO_ERROR)
        return ret_val;

    if ((ret_val = trim_start(arg)) == NO_ERROR)
        ret_val = string_to_llint(arg, &arg_lli);

    if (arg_lli < 0 || arg_lli >= NUMBER_OF_TEMPORARY_VARIABLES)
        ret_val = COMMAND_ERROR;


    if (ret_val == NO_ERROR)
    {
        switch (findex)
        {
            // def _X
            case 0:
                if ((selector->lld_ir1 != selector->lld_ir2) || (selector->lld_ic1 != selector->lld_ic2))
                {
                    ret_val = COMMAND_ERROR;
                    break;
                }

                ret_val = set_temporary_variable(table, selector, temp_var_store, arg_lli);
                break;

            // use _X
            case 1:
                ret_val = set_cell_from_temporary_variable(table, selector, temp_var_store, arg_lli);
                break;

            // inc _X
            case 2:
                ret_val = increase_temporary_variable(temp_var_store, arg_lli);
                break;

            default:
                ret_val = COMMAND_ERROR;
                break;
        }
    }

    free(arg);

    return ret_val;
}

int get_type_of_command(Base_command *command)
{
    /**
     * @brief Get type of inputed @p command
     *
     * @param command Pointer to instance of #Base_command structure that will be tested
     *
     * @return Type of command based on #CommandType
     */

    if (command == NULL)
        return UNKNOWN;

    if (is_table_editing_command(command))
        return TABLE_EDITING_COMMAND;

    if (is_data_editing_command(command))
        return DATA_EDITING_COMMAND;

    if (is_temp_var_command(command))
        return TEMP_VAR_COMMAND;

    return UNKNOWN;
}

int execute_commands(Table *table, Base_commands *base_commands_store)
{
    /**
     * @brief Execute commands on @p table
     *
     * Iterate over all commands in @p base_commands_store and parse them and execute them on @p table
     *
     * @param table Pointer to instance of #Table structure
     * @param base_commands_store Pointer to instance of #Base_commands structure
     *
     * @return #NO_ERROR on success in other cases coresponding error code from #ErrorCodes
     */

    int ret_val = NO_ERROR;

    Raw_selector selector = { .initialized = false };
    Raw_selector temp_selector = { .initialized = false };
    init_selector(&selector);
    init_selector(&temp_selector);

    TempVariableStore temp_var_store = { .variables = NULL };
    if (init_temp_var_store(&temp_var_store) != NO_ERROR)
        return ALLOCATION_FAILED;

    for (long long int i = 0; i < base_commands_store->num_of_commands; i++)
    {
        Base_command c_comm = base_commands_store->commands[i];
        if (is_command_selector(&c_comm))
        {
            // Set new selector
            if ((ret_val = set_selector(&selector, &temp_selector, &c_comm, table)) != NO_ERROR)
                break;
            continue;
        }

#ifdef DEBUG
        printf("Current selector: [%llu,%llu,%llu,%llu]\n", selector.lld_ir1, selector.lld_ic1, selector.lld_ir2, selector.lld_ic2);
        printf("Current command: [%s,%s]\n", c_comm.function, c_comm.arguments);
        printf("Current variable store:\n[");
        for (long long int j = 0; j < NUMBER_OF_TEMPORARY_VARIABLES; j++)
        {
            printf("_%llu:'%s',", j, temp_var_store.variables[j]);
        }
        printf("]\n\n");
        printf("Before table:\n");
        print_table(table);
#endif

        switch (get_type_of_command(&c_comm))
        {
            case TABLE_EDITING_COMMAND:
                ret_val = execute_table_editing_comm(table, &selector, &c_comm);
                break;

            case DATA_EDITING_COMMAND:
                if (table->num_of_rows > 0 && table->rows[0].num_of_cells > 0)
                    ret_val = execute_data_editing_command(table, &selector, &c_comm);
                break;

            case TEMP_VAR_COMMAND:
                if (table->num_of_rows > 0 && table->rows[0].num_of_cells > 0)
                    ret_val = execute_temp_var_command(table, &selector, &c_comm, &temp_var_store);
                break;

            default:
                ret_val = COMMAND_ERROR;
        }

        if (ret_val != NO_ERROR)
            break;

#ifdef DEBUG
        printf("\nAfter table:\n");
        print_table(table);
        printf("\n#####################################################\n\n");
#endif
    }

    deallocate_temp_var_store(&temp_var_store);

    return ret_val;
}

int main(int argc, char *argv[]) {
    /**
     * @brief Main of whole program
     * @todo Refactor error handling - separate to own function
     */

    int error_flag;

    // Check if number of arguments is larger than minimum posible number of arguments
    if (argc < 3) {
        fprintf(stderr, "Some arguments are missing!\n");
        return MISSING_ARGS;
    }

    _Bool delim_flag_present = strings_equal(argv[1], "-d");
    char *delims = delim_flag_present ? argv[2] : DEFAULT_DELIM;

    // Check if there is no invalid characters in delim array
    if (!check_sanity_of_delims(delims))
    {
        fprintf(stderr, "Cant found valid delimiters after -d flag\n");
        return INVALID_DELIMITER;
    }

    Raw_commands raw_commands_store = { .commands = NULL,
                                        .num_of_commands = 0 };

    if ((error_flag = get_commands(argv, &raw_commands_store, delim_flag_present)) != NO_ERROR)
    {
        fprintf(stderr, "Failed to get commands\n");
        deallocate_raw_commands(&raw_commands_store);
        return error_flag;
    }

    Base_commands base_commands_store = { .commands = NULL,
                                          .num_of_commands = 0 };

    if ((error_flag = parse_commands(&raw_commands_store, &base_commands_store)) != NO_ERROR)
    {
        fprintf(stderr, "Failed to parse commands\n");
        deallocate_raw_commands(&raw_commands_store);
        deallocate_base_commands(&base_commands_store);
        return error_flag;
    }

    deallocate_raw_commands(&raw_commands_store);

    Table table = { .delim = delims[0],
                    .rows = NULL,
                    .num_of_rows = 0,
                    .allocated_rows = 0};

    if (((error_flag = load_table(delims, argv[argc-1], &table)) != NO_ERROR))
    {
        fprintf(stderr, "Failed to load table properly\n");
        deallocate_table(&table);
        deallocate_base_commands(&base_commands_store);
        return error_flag;
    }

    if (table.rows != NULL)
    {
        if ((error_flag = normalize_number_of_cols(&table)) != NO_ERROR)
        {
            fprintf(stderr, "Failed to normalize colums\n");
            deallocate_table(&table);
            deallocate_base_commands(&base_commands_store);
            return error_flag;
        }

        if ((error_flag = execute_commands(&table, &base_commands_store)) != NO_ERROR)
        {
            fprintf(stderr, "Failed to execute all commands\n");
            deallocate_table(&table);
            deallocate_base_commands(&base_commands_store);
            return error_flag;
        }
    }

#ifdef DEBUG
    printf("\nFinal table:\n");
    print_table(&table);
    printf("\nAdditional info:\n");
    printf("Allocated rows: %llu, Allocated cells: %llu\n", table.allocated_rows, table.rows[0].allocated_cells);
    printf("Delim: '%c'\n", table.delim);
    printf("Commands (%lld): ", base_commands_store.num_of_commands);
    for (long long int i = 0; i < base_commands_store.num_of_commands; i++)
        printf("'Com[f(%s), arg(%s)]' ", base_commands_store.commands[i].function, base_commands_store.commands[i].arguments);
    printf("\n");
    printf("Raw Args: ");
    for (int i = 1; i < argc; i++)
        printf("%s%c", argv[i], i == (argc - 1) ? '\n' : ' ');
#else
    save_table(&table, argv[argc-1]);
#endif

    deallocate_table(&table);
    deallocate_base_commands(&base_commands_store);

    return NO_ERROR;
}
