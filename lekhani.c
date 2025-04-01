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
static const char *VERSION = "0.0.1";
static const char *AUTHOR = "Khethan R G";
static const char *LICENSE_URL = "https://github.com/khethan-god/Lekhani/blob/main/LICENSE";

/*** Macros ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

/*** Enums ***/
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*** Data Structures ***/
struct editorConfig {
    int cx, cy;         // Cursor position
    int screenRows;     // Number of rows in the terminal
    int screenCols;     // Number of columns in the terminal
    struct termios orig_termios; // Original terminal settings
};

struct abuf {
    char *b;            // Buffer data
    int len;            // Buffer length
};

/*** Global Data ***/
static struct editorConfig E;

/*** Append Buffer Functions ***/

/*
 * Appends a string to the append buffer.
 * Args:
 *   ab - Pointer to the append buffer.
 *   s - String to append.
 *   len - Length of the string to append.
 */
static void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

/*
 * Frees the memory allocated for the append buffer.
 * Args:
 *   ab - Pointer to the append buffer.
 */
static void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** Error Handling ***/

/*
 * Prints an error message to stderr and exits with failure status.
 * Clears the screen before exiting for a clean termination.
 * Args:
 *   msg - The error message to display before the system error description.
 */
static void die(const char *msg) {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor to top-left
    perror(msg);
    exit(EXIT_FAILURE);
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
    raw.c_cc[VTIME] = 0.1; // 0.1 seconds timeout for read
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

/*
 * Reads a single keypress from the terminal, handling escape sequences.
 * Returns:
 *   The key code (e.g., a character or an enum value like ARROW_UP).
 */

static int editorReadKey(void) {
    char c;
    int nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    // Handle escape sequences (e.g., arrow keys)
    if (c == '\x1b') {
        char seq[3] = {0};
        
        // Read next two bytes if available, timeout otherwise
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        // Parse ANSI escape codes
        if (seq[0] == '[') {
            // Handle sequences like "[3~" (DEL), "[5~" (PAGE_UP), etc.
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;   // DEL key; currently does nothing
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b'; // Unrecognized escape sequence
    }
    return c; // Regular character
}

/*
 * Queries the terminal for the cursor position using the `n` command
 * Args:
 *   rows - Pointer to store the row number.
 *   cols - Pointer to store the column number.
 * Returns:
 *   0 on success, -1 on failure.
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
 * Retrieves the terminal window size.
 * Uses ioctl if available, falls back to cursor positioning method.
 * Args:
 *   rows - Pointer to store the number of rows.
 *   cols - Pointer to store the number of columns.
 * Returns:
 *   0 on success, -1 on failure.
 */
static int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
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
    if (argc < 2) return false;
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printVersion();
        return true;
    }
    return false;
}

/*** Output functions ***/

/*
 * Draws the editor rows with tildes and a welcome message.
 * Args:
 *   ab - Pointer to the append buffer to store the output.
 */
static void editorDrawRows(struct abuf *ab) {
    for (int y = 0; y < E.screenRows; y++) {
        if (y == E.screenRows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                                     "Lekhani editor -- version %s", VERSION);
            if (welcomelen > E.screenCols) welcomelen = E.screenCols;
            int padding = (E.screenCols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }
        abAppend(ab, "\x1b[K", 3); // Clear line from cursor to end
        if (y < E.screenRows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

/*
 * Refreshes the editor screen by drawing rows and updating cursor position.
 */
static void editorRefreshScreen(void) {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
    abAppend(&ab, "\x1b[H", 3);    // Move cursor to top-left
    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf)); // Move cursor to current position

    abAppend(&ab, "\x1b[?25h", 6); // Show cursor
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** Input Functions ***/

/*
 * Moves the cursor based on the given key.
 * Args:
 *   key - The key code (e.g., ARROW_LEFT) to process.
 */
static void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) E.cx--;
            break;
        case ARROW_RIGHT:
            if (E.cx < E.screenCols - 1) E.cx++;
            break;
        case ARROW_UP:
            if (E.cy > 0) E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy < E.screenRows - 1) E.cy++;
            break;
    }
}

/*
 * Processes a single keypress and updates editor state.
 */
static void editorProcessKeyPress() {
    int c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case HOME_KEY:  // moves cursor to left edge of the screen
            E.cx = 0;
            break; 
        case END_KEY:   // moves cursor to right edge of the screen
            E.cx = E.screenCols - 1;
            break; 
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenRows;
                while (times--) {
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}

/*** Initialization ***/

/*
 * Initializes the editor configuration with screen size and cursor position.
 */
static void initEditor(void) {
    E.cx = 1;
    E.cy = 0;
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1) {
        die("getWindowSize");
    }
}

/*** Entry Point ***/

/*
 * Main function: initializes and runs the editor.
 * Args:
 *   argc - Number of command-line arguments.
 *   argv - Array of argument strings.
 * Returns:
 *   EXIT_SUCCESS on successful execution.
 */
int main(int argc, char *argv[]) {
    if (checkVersionFlag(argc, argv)) {
        return EXIT_SUCCESS;
    }

    enableRawMode();
    initEditor();
    while (true) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }

    return EXIT_SUCCESS;
}