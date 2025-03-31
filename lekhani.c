#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

/*
 *
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *  https://github.com/khethan-god/Lekhani/blob/main/LICENSE for full details.
*/

struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    // restore the original terminal attributes
}

void print_version() {
    printf("Lekhani v1.0.0 Copyright (C) 2025 Khethan R G\n");
    printf("This is a free software. This program comes with ABSOLUTELY NO WARRANTY;\n");
    printf("not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
    printf("You are welcome to redistribute it under GNU GPL v3\n");
    printf("Full license: https://github.com/khethan-god/Lekhani/blob/main/LICENSE \n");
}

int version(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--version") == 0) print_version();
    return 0;
}

void enableRawMode() {
    // we will use this function to enable raw mode in the terminal
    // which will allow us to send every key press to the program 
    // without waiting for the enter key
    tcgetattr(STDIN_FILENO, &orig_termios);  // taking the original terminal attributes
    atexit(disableRawMode);  // this is called when the program exits automatically
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON); // this line disables printing of the input (key press) in the terminal, and turns off canonical mode -> reading i/p byte-by-byte instead of line-by-line
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    // TCSAFLUSH: flush the input buffer and set the new attributes
}

int main(int argc, char *argv[]) {
    version(argc, argv);
    enableRawMode();

    int i;
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c!='q') // `q` -> quit
    {
        if (iscntrl(c)) printf("%d\n", c);
        else printf("%d ('%c')\n", c, c);
        // iscntrl: check if the character is a control character
        // isprint: check if the character is a printable character
    }
    printf("Read %d characters from stdin\n", i);
    return 0;
}