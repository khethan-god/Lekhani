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

/*** Constants ***/
static const char *VERSION = "1.0.0";
static const char *AUTHOR = "Khethan R G";
static const char *LICENSE_URL = "https://github.com/khethan-god/Lekhani/blob/main/LICENSE";

/*** Global Data ***/
static struct termios orig_termios;

/*** Error Handling ***/

/*
 * Prints an error message to stderr and exits with failure status.
 * Args:
 *   msg - The error message to display before the system error description.
 */
static void die(const char *msg) {
    perror(msg);
    exit(1);
}

/*** Terminal Functions ***/

/*
 * Restores the terminal to its original settings.
 * Disables raw mode by resetting the terminal attributes to their saved state.
 */
static void disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}

/*
 * Configures the terminal to raw mode for character-by-character input.
 * Saves original terminal settings and disables canonical mode, echo, and signals.
 */
static void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    }
    
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
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

/*
 * Runs the main editor loop, reading and displaying input characters.
 * Exits when 'q' is pressed and reports the total characters read.
 */
static void editorLoop(void) {
    int char_count = 0;
    char input;
    
    while (true) {
        input = '\0';
        if (read(STDIN_FILENO, &input, 1) == -1 && errno != EAGAIN) {
            die("read");
        }
        
        char_count++;
        
        if (iscntrl(input)) {
            printf("%d\r\n", input);
        } else {
            printf("%d ('%c')\r\n", input, input);
        }
        
        if (input == 'q') {
            break;
        }
    }
    
    printf("Read %d characters from stdin\r\n", char_count);
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
    editorLoop();
    
    return 0;
}