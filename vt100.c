#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <ncurses.h>
#include "vtparse.h"



static struct termios orig_termios;


void term_restore(void)
{
//        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        endwin();
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
        atexit(term_restore);
        raw();
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
                if (parser->params[0] <= maxy) {
                        move(parser->params[0] + 1, x);
                        refresh();
                }
                return;
        }

        if (parser->num_params == 2) {
                if (parser->params[0] <= maxy &&
                    parser->params[1] <= maxx) {
                        move(parser->params[0] + 1, parser->params[1] + 1);
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
        const char *PROGRAM_B_INPUT = "fake input\n";
        char *PROGRAM_B = argv[1];

        vtparse_t parser;

        term_ctx_t ctx;

        term_config();

        vtparse_init(&parser, parser_callback);

        error(0,0,"Starting [%s]...", PROGRAM_B);
        //if ((pipe(fd1) < 0) || (pipe(fd2) < 0)) {
        if (pipe(fd2) < 0) {
                error(0, 0, "PIPE ERROR");
                return -2;
        }

        if ((pid = fork()) < 0) {
                error(0,0,"FORK ERROR");
                return -3;
        } else if (pid == 0) {     // CHILD PROCESS
                // close(fd1[1]);
                close(fd2[0]);

                /* if (dup2(fd1[0], STDIN_FILENO) != STDIN_FILENO) {
                        error(0,0,"-- CHILD --    dup2 error to stdin");
                }
                close(fd1[0]); */

                if (dup2(fd2[1], STDOUT_FILENO) != STDOUT_FILENO) {
                        error(0,0,"-- CHILD --    dup2 error to stdout");
                }
                close(fd2[1]);

                if (execvp(argv[1], argv + 1) < 0) {
                        error(0,0,"-- CHILD --    system error");
                        perror("ERROR DEFN : ");
                        return -4;
                }

                return 0;
        } else {        // PARENT PROCESS
                int rv;
                // close(fd1[0]);
                close(fd2[1]);

                /* if (write(fd1[1], PROGRAM_B_INPUT, strlen(PROGRAM_B_INPUT)) != strlen(PROGRAM_B_INPUT)) {
                        error(0,0,"READ ERROR FROM PIPE");
                } */

next_data:
                if ((rv = read(fd2[0], line, MAXLINE)) < 0) {
                        error(0,0,"READ ERROR FROM PIPE");
                } else if (rv == 0) {
                        error(0,0,"Child Closed Pipe");
                        /* FIXME: waitpid */

                        sleep(3);
                        return 0;
                }

                vtparse(&parser, (unsigned char *)line, rv);

                goto next_data;

        }

        return 0;
}
