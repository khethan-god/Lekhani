/*
 *  Lekhani: A text editor
 *  Copyright (C) 2025  Khethan R G
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see https://www.gnu.org/licenses/.
 * 
 *  Full license: https://github.com/khethan-god/Lekhani/blob/main/LICENSE
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <stdbool.h>
#include <sys/ioctl.h>

/*** Constants ***/
static const char *VERSION = "1.0.0";
static const char *AUTHOR = "Khethan R G";
static const char *LICENSE_URL = "https://github.com/khethan-god/Lekhani/blob/main/LICENSE";

/* 
 * Macros
 */
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

/*** Global Data ***/
struct editorConfig {
    int screenRows;
    int screenCols;
    struct termios orig_termios;
};

static struct editorConfig E;

// It is normally not good practice to write every character to the screen ( which is happening in the draw cursor function), so we are going to create a buffer and append all the characters to the buffer and then write the entire buffer to the screen at once, this is called double buffering, and it is used to reduce flickering and improve performance.
// We will create a dynamic array to perform this operation

/*** Append Buffer ***/

struct abuf {
    char *b;
    int len;
};

/*
 * Appends a string to the buffer.
 * Args:
 *   ab - Pointer to the append buffer.
 *   s - String to append.
 *   len - Length of the string to append.
 */
static void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len + len);
    if (new ==NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

/*
 * Clears the buffer by freeing the allocated memory. No need to free the len as it
 * is a primitive type and will be automatically freed when the buffer is freed.
 * This function is used to free the memory allocated for the buffer when it is no longer needed.
 * Args:
 *   ab - Pointer to the append buffer.
 */
static void abFree(struct abuf *ab) {
    free(ab->b);
}


/*** Error Handling ***/

/*
 * Prints an error message to stderr and exits with failure status.
 * Args:
 *   msg - The error message to display before the system error description.
 */
static void die(const char *msg) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(msg);
    exit(1);
}

/*** Terminal Functions ***/

/*
 * Restores the terminal to its original settings.
 * Disables raw mode by resetting the terminal attributes to their saved state.
 */
static void disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

/*
 * Configures the terminal to raw mode for character-by-character input.
 * Saves original terminal settings and disables canonical mode, echo, and signals.
 */
static void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    
    atexit(disableRawMode);
    
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

/*
 * Reads a single character from standard input.
 * Blocks until a character is available, then returns it.
 * Since control characters are disabled, no need to check for special cases
 * using iscntrl().
 * Returns:
 *   The character read from standard input.
 */
static char editorReadKey() {
    int nread;
    char c;
    
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read");
        }
    }
    
    return c;
}

/*
 * Gets the current cursor position in the terminal using the n command to query the terminal for status information (6 -> returns the cursor position), then use this information to determine the current row and column position of the cursor.
 * This is done by sending the escape sequence "\x1b[6n" to the terminal, which requests the cursor position.
 * The terminal responds with a string in the format "\x1b[<rows>;<cols>R", where <rows> and <cols> are the current row and column positions, respectively.
 * Args:
 *   rows - Pointer to store the current row position.
 *   cols - Pointer to store the current column position.
 * Returns:
 *   -1 on success or failure.
 */

static int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;

}

/*
 * Gets the size of the terminal window.
 * Args:
 *   rows - Pointer to store the number of rows.
 *   cols - Pointer to store the number of columns.
 * 
 * since this function is uses pointers, the address of the rows and cols need
 * not be passed, we can just dereference the pointers and assign the values,
 * 
 * sometimes ioctl() may fail and not return the exact number of rows and cols,
 * like it might be one less, or it might start from 0 (a line exactly above the
 * first line) not displayed in the terminal, so we are going to manually calculate
 * the window size as well by moving the cursor to the bottom-right corner of the
 * terminal and querying the position of the cursor. 
 * 
 */
static int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** Version Display ***/

/*
 * Prints the program's version, copyright, and licensing information.
 */
static void printVersion(void) {
    printf("Lekhani v%s Copyright (C) 2025 %s\n", VERSION, AUTHOR);
    printf("This is free software. This program comes with ABSOLUTELY NO WARRANTY;\n");
    printf("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
    printf("You are welcome to redistribute it under GNU GPL v3\n");
    printf("Full license: %s\n", LICENSE_URL);
}

/*
 * Checks if the command-line arguments include a version flag.
 * Args:
 *   argc - Number of command-line arguments.
 *   argv - Array of command-line argument strings.
 * Returns:
 *   true if "--version" or "-v" is present (and prints version), false otherwise.
 */
static bool checkVersionFlag(int argc, char *argv[]) {
    if (argc < 2) {
        return false;
    }
    
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printVersion();
        return true;
    }
    return false;
}

/*** Main Editor Loop ***/

/*** Output ***/


/*
 * Draws the rows of the editor.
 * This function uses a dynamic array to first store all the tilde characters and then
 * prints them to the screen at once.
 */
static void editorDrawRows(struct abuf *ab) {
    // this function is used to draw ~ characters in the first column of the editor
    int y;
    for (y = 0; y < E.screenRows; y++) {
        abAppend(ab, "~", 1);
        abAppend(ab, "\x1b[k", 3); // clear the line from the cursor to the end of the line

        if (y < E.screenRows - 1) {
            abAppend(ab, "\r\n", 2); // this makes sure that the last line
            // in the editor is not followed by a new line, so the entire screen
            // has tilde characters
        }
    }
}

void editorRefreshScreen() {
    // screen refresh logic is used to render the editor's UI to the screen
    // after each keypress
    // 0. create a buffer to store the output
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // hide the cursor. Why? sometimes the cursor might be
    // blinking when we are writing the arrows on the screen which is not how an editor 
    // should behave, so we are going to hide the cursor and then show it again when we are
    // done drawing the screen.


    // 1. clear the screen
    // \x1b[2J clear screen and send cursor to home position.
    // \x1b[H send cursor to cursor home position.
    // <esc>[1J would clear the screen upto the cursor position.
    // <esc?[0J would clear the screen from the cursor upto the end of the screen.
    // abAppend(&ab, "\x1b[2J", 4);  // clears the screen by moving any text to the top
    // But we are not going to use this because it is more optimal to clear
    // each line as we redraw it. (done in editor Draw Rows).
    // 2. move the cursor to the top-left corner
    abAppend(&ab, "\x1b[H", 3);
    // 3. draw the rows
    editorDrawRows(&ab);
    // 4. move the cursor to the top-left corner
    abAppend(&ab, "\x1b[H", 3);

    abAppend(&ab, "\x1b[?25h", 6); // show the cursor again

    write(STDOUT_FILENO, ab.b, ab.len); // write the buffer to the screen
    // 5. flush the output
    abFree(&ab); // free the buffer
}

/*** Input: This section maps keys to editor functions ***/

/*
 * Runs within the main editor loop, reading input characters.
 * Exits when 'CTRL + q' is pressed.
 */
static void editorProcessKeyPress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*** Initialize ***/

/*
 * Uses getWindowSize to set the screenRows and screenCols in the editorConfig struct.
 */
static void initEditor() {
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
        die("getWindowSize");
    }
}

/*** Entry Point ***/

/*
 * Main function: initializes the program and starts the editor.
 * Args:
 *   argc - Number of command-line arguments.
 *   argv - Array of command-line argument strings.
 * Returns:
 *   0 on successful execution.
 */
int main(int argc, char *argv[]) {
    if (checkVersionFlag(argc, argv)) {
        return 0;
    }
    
    enableRawMode();
    initEditor();
    while (1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return 0;
}