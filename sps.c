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

#define DEBUG

#define BLACKLISTED_DELIMS "\"\\" /**< Character that are not allowed to use as delim character */
#define DEFAULT_DELIM " " /**< Default delimiter array */

#define BASE_NUMBER_OF_ROWS 3 /**< Base number of rows that will be allocated */
#define BASE_NUMBER_OF_CELLS 3 /**< Base number of cells that will be allocated */
#define BASE_CELL_LENGTH 6 /**< Base length of content in cell that will be allocated */

#define EMPTY_CELL "" /**< How should look like empty cell */

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
    COMMAND_ERROR,                /**< Error when received invalid commands - 8 */
};

/**
 * @struct Commands
 * @brief Store raw commands in form of string array
 */
typedef struct
{
    long long int num_of_commands;
    char **commands; /**< Array of commands */
} Raw_commands;

typedef struct
{
    char *function;
    char *arguments;
} Base_command;

typedef struct
{
    long long int num_of_commands;
    Base_command *commands;
} Base_commands;

/**
 * @struct Cell
 * @brief Store for data of single cell
 */
typedef struct
{
    long long int allocated_chars;
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
    long long int counter = 0;

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

    for (size_t i = 0; string[i]; i++)
    {
        if (string[i] == '"')
            in_parentecies = !in_parentecies;

        if (string[i] == ch)
        {
            if (ignore_escapes || (!in_parentecies && (i != 0 && string[i-1] != '\\')))
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

void dealocate_cell(Cell *cell)
{
    /**
     * @brief Dealocate cell
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

void dealocate_row(Row *row)
{
    /**
     * @brief Dealocate row
     *
     * Iterate over all cells and dealocate them
     *
     * @param row Pointer to instance of #Row structure
     */

    if (row->cells == NULL)
        return;

    for (long long int i = 0; i < row->num_of_cells; i++)
    {
        dealocate_cell(&row->cells[i]);
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
        dealocate_row(&table->rows[i]);
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
            if ((ret_val = get_substring(raw_command_store->commands[i], &substring_buf, ' ', 0, false, &rest, true)) != NO_ERROR)
                return ret_val;

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

    if (table->rows == NULL)
        return FUNCTION_ERROR;

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

    if (cell->content == NULL)
    {
        // Allocate base cell
        if (allocate_content(cell) != NO_ERROR)
            return ALLOCATION_FAILED;
    }

    while ((strlen(string) + 1) > (unsigned long long)cell->allocated_chars)
        if (allocate_content(cell) != NO_ERROR)
            return ALLOCATION_FAILED;

    strcpy(cell->content, string);

    return NO_ERROR;
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
                    dealocate_cell(&table->rows[j].cells[i]);
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

        dealocate_cell(&table->rows[i].cells[j]);
        table->rows[i].num_of_cells--;
    }

    return NO_ERROR;
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

    dealocate_row(&table->rows[index]);

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

    if (((error_flag = load_table(delims, argv[argc-1], &table)) != NO_ERROR) || table.rows == NULL)
    {
        fprintf(stderr, "Failed to load table properly\n");
        deallocate_table(&table);
        deallocate_base_commands(&base_commands_store);
        return error_flag;
    }

    if ((error_flag = normalize_number_of_cols(&table)) != NO_ERROR)
    {
        fprintf(stderr, "Failed to normalize colums\n");
        deallocate_table(&table);
        deallocate_base_commands(&base_commands_store);
        return error_flag;
    }

#ifdef DEBUG
    print_table(&table);
    printf("\n\nDebug:\n");
    printf("Allocated rows: %llu, Allocated cells: %llu\n", table.allocated_rows, table.rows[0].allocated_cells);
    printf("Delim: '%c'\n", table.delim);
    printf("Number of commands: %lld\n", base_commands_store.num_of_commands);
    printf("Commands: ");
    for (long long int i = 0; i < base_commands_store.num_of_commands; i++)
        printf("'%s - %s' ", base_commands_store.commands[i].function, base_commands_store.commands[i].arguments);
    printf("\n");
    printf("Args: ");
    for (int i = 1; i < argc; i++)
        printf("%s%c", argv[i], i == (argc - 1) ? '\n' : ' ');
#else
    save_table(&table, argv[argc-1]);
#endif

    deallocate_table(&table);
    deallocate_base_commands(&base_commands_store);

    return NO_ERROR;
}
