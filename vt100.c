#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <pty.h>

#include <ncurses.h>
#include "vtparse.h"



static struct termios orig_termios;


void term_restore(void)
{
//        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        endwin();
}


static void handle_resizing(void)
{
        endwin();
        initscr();
        keypad(stdscr, true);
        cbreak();
        noecho();
}


void term_config(void)
{
        if (!isatty(STDIN_FILENO)) {
                perror("Not a tty");
                exit(-1);
        }

/*
        struct winsize {
                unsigned short ws_row;
                unsigned short ws_col;
                unsigned short ws_xpixel;
                unsigned short ws_ypixel;
        };

        struct winsize w;
        w.ws_row = 24;
        w.ws_col = 80;

        ioctl(STDIN_FILENO, TIOCSWINSZ, &w);


/*        if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) {
                perror("Can't get terminal settings");
                exit(-1);
        }
*/
        initscr();
        //newterm();
        atexit(term_restore);
        keypad(stdscr, true);
        cbreak();
        noecho();
        move(0, 0);
        curs_set(1);
        refresh();
/*
        struct termios raw;

        raw = orig_termios;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);

        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
*/

}


typedef struct {
        int dummy;
} term_ctx_t;


/* here we use ansi escape sequences to move the cursor LOL (quick test) */

void cur_clear(term_ctx_t *ctx, vtparse_t *parser)
{
        if (parser->num_params == 0) {
                clrtobot();
                return;
        }
        if (parser->num_params == 1) {
                switch(parser->params[0]) {
                case 0:
                        clrtobot();
                        return;
                case 1:
                        /* not implemented */
                        return;
                case 2:
                        clear();
                        return;
                }
        }

}


void cur_attr(term_ctx_t *ctx, vtparse_t *parser)
{
        if (parser->num_params == 0) {
                attrset(WA_NORMAL);
                return;
        }
        if (parser->num_params == 1) {
                switch(parser->params[0]) {
                case 0:
                        attrset(WA_NORMAL);
                        return;
                case 1:
                        attrset(WA_BOLD);
                        return;
                case 2:
                        attrset(WA_DIM);
                        return;
                case 4:
                        attrset(WA_UNDERLINE);
                        return;
                case 5:
                        attrset(WA_BLINK);
                        return;
                case 7:
                        attrset(WA_REVERSE);
                        return;
                default:
                        return;
                }
        }
}


void cur_up(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        getyx(stdscr, y, x);
        if (parser->num_params == 0) {
                if (y > 0) {
                        y--;
                        move(y, x);
                        refresh();
                }

        } else {
                if (y - parser->params[0] >= 0) {
                        y -= parser->params[0];
                        move(y, x);
                        refresh();
                }
        }
}


void cur_down(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        int maxx, maxy;
        getmaxyx(stdscr, maxy, maxx);
        getyx(stdscr, y, x);
        if (parser->num_params == 0) {
                if (y < maxy) {
                        y++;
                        move(y, x);
                        refresh();
                }

        } else {
                if (y + parser->params[0] <= maxy) {
                        y += parser->params[0];
                        move(y, x);
                        refresh();
                }
        }
}


void cur_right(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        int maxx, maxy;
        getmaxyx(stdscr, maxy, maxx);
        getyx(stdscr, y, x);
        if (parser->num_params == 0) {
                if (x < maxx) {
                        x++;
                        move(y, x);
                        refresh();
                }
        } else {
                if (x + parser->params[0] <= maxx) {
                        x += parser->params[0];
                        move(y, x);
                        refresh();
                }
        }
}


void cur_left(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        getyx(stdscr, y, x);
        if (parser->num_params == 0) {
                if (x > 0) {
                        x--;
                        move(y, x);
                        refresh();
                }

        } else {
                if (x - parser->params[0] >= 0) {
                        x -= parser->params[0];
                        move(y, x);
                        refresh();
                }
        }
}


void cur_home(term_ctx_t *ctx, vtparse_t *parser)
{
        if (parser->num_params == 0) {
                move(0, 0);
                refresh();
                return;
        }

        int x, y;
        int maxx, maxy;
        getyx(stdscr, y, x);
        getmaxyx(stdscr, maxy, maxx);
        if (parser->num_params == 1) {
                if (parser->params[0] <= maxy + 1) {
                        move(parser->params[0] - 1, x);
                        refresh();
                }
                return;
        }

        if (parser->num_params == 2) {
                if (parser->params[0] <= maxy + 1 &&
                    parser->params[1] <= maxx + 1) {
                        move(parser->params[0] - 1, parser->params[1] - 1);
                        refresh();
                }
                return;
        }
}


void term_put(term_ctx_t *ctx, vtparse_t *parser, unsigned int ch)
{
        waddch(stdscr, ch);
        refresh();
}


void cur_newline()
{
        int maxx, maxy, x, y;

        getmaxyx(stdscr, maxy, maxx);
        getyx(stdscr, y, x);
        if (y + 1 <= maxy) {
                move(y + 1, 0);
        }
        refresh();
}


void parser_callback(vtparse_t *parser, vtparse_action_t action, unsigned int ch)
{
        /* VT100 passive display support */
        if (parser->state == VTPARSE_STATE_GROUND) {
                switch (action) {
                default:
                        break;
                }
        }

        switch(action) {
        case VTPARSE_ACTION_EXECUTE:
                switch (ch) {
                case 7:
                        break;
                case 10:
                        /* new line */
                        cur_newline();
                        break;
                case 13:
                        // printf("%c", ch);
                        // cur_down(parser->user_data, parser);
                        break;
                default:
                        break;
                }
                break;
//         case VTPARSE_ACTION_ESC_DISPATCH:
        case VTPARSE_ACTION_CSI_DISPATCH:
                // printf("%c\n", ch);
                switch (ch) {
                case 'A':
                        cur_up(parser->user_data, parser);
                        break;
                case 'B':
                        cur_down(parser->user_data, parser);
                        break;
                case 'C':
                        cur_right(parser->user_data, parser);
                        break;
                case 'D':
                        cur_left(parser->user_data, parser);
                        break;
                case 'f':
                case 'H':
                        cur_home(parser->user_data, parser);
                        break;
                case 'm':
                        cur_attr(parser->user_data, parser);
                        break;
                case 'J':
                        cur_clear(parser->user_data, parser);
                        break;
                default:
                        break;
                }
                break;
        case VTPARSE_ACTION_PRINT:
                term_put(parser->user_data, parser, ch);
                break;
        default:
                break;
        }
}


int main(int argc, char *argv[])
{
        int MAXLINE = 2000;
        int fd1[2];
        int fd2[2];
        pid_t pid;
        char line[MAXLINE];

        vtparse_t parser;

        term_ctx_t ctx;

        term_config();

        vtparse_init(&parser, parser_callback);
        int master;
        pid = forkpty(&master, NULL, NULL, NULL);

        if (pid == -1) {
                printf("Error with forkpty");
                return -1;
        }

        if (!pid) {
                char* exec_argv[] = {"/usr/bin/mc", NULL};
                char *env[] = {
                        "PATH=/usr/bin:/bin",
                        "HOME=/home/andreas",
                        "TERM=vt100",
                        "LC_ALL=C",
                        "USER=andreas",
                        "COLUMNS=100",
                        "ROWS=25",
                        0
                };
                execve(exec_argv[0], exec_argv, env);
                perror("CHILD: execvp");
                return EXIT_FAILURE;
        } else {

                /* main pid */

                nodelay(stdscr, true);
                while (1) {
                        int rcv;

                        char buffer[128];
                        refresh();
                        doupdate();

                        /* look if something is there on child's stdout */
                        struct timeval timeout;
                        timeout.tv_sec = 0;
                        timeout.tv_usec = 100;
                        fd_set fds;
                        FD_ZERO(&fds);
                        FD_SET(master, &fds);

                        int rc = select(master + 1, &fds, NULL, NULL, &timeout);
                        if (rc > 0) {
                                rcv = read(master, buffer, sizeof(buffer));
                                if (rcv > 0) {
                                        vtparse(&parser, (unsigned char *)buffer, rcv);
                                }
                                refresh();
                                doupdate();
                        }
                        int in = getch();
                        if (in != ERR) {
                                char keybuff[4];
                                if (in == KEY_RESIZE) {
                                        handle_resizing();
                                        struct winsize {
                                                unsigned short ws_row;
                                                unsigned short ws_col;
                                                unsigned short ws_xpixel;
                                                unsigned short ws_ypixel;
                                        };

                                        struct winsize w;
                                        int maxx, maxy;
                                        getmaxyx(stdscr, maxy, maxx);
                                        w.ws_row = maxy;
                                        w.ws_col = maxx;
                                        ioctl(master, TIOCSWINSZ, &w);
                                        continue;
                                }
                                if (in == KEY_UP) {
                                        sprintf(keybuff, "\033OA");
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_DOWN) {
                                        sprintf(keybuff, "\033OB");
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_RIGHT) {
                                        sprintf(keybuff, "\033OC");
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_LEFT) {
                                        sprintf(keybuff, "\033OD");
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in >= 1 && in < 256) {
                                        write(master, &in, 1);
                                        continue;
                                }
                                if (in >= 256 && in < 256*256 - 1) {
                                        write(master, &in, 2);
                                        continue;
                                }
                                if (in > 256*256 && in < 256*256*256 - 1) {
                                        write(master, &in, 3);
                                        continue;
                                }
                        }
                }
        }
        if (waitpid(pid, NULL, 0) == -1) {
                perror("PARENT: waitpid");
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}
