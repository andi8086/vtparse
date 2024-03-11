#define _XOPEN_SOURCE_EXTENDED
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
#include <locale.h>

#include <curses.h>
#include "vtparse.h"


static struct termios orig_termios;

typedef enum {
        CURSOR_NORMAL = 0, 
        CURSOR_APP
} cursor_mode_t;

typedef enum {
        KEYPAD_NORMAL = 0, 
        KEYPAD_APP
} keypad_mode_t;

const char *key_up_seq[2] = {"\033[A", "\033OA"};
const char *key_down_seq[2] = {"\033[B", "\033OB"};
const char *key_right_seq[2] = {"\033[C", "\033OC"};
const char *key_left_seq[2] = {"\033[D", "\033OD"};


const char *DEC_special_as_utf8[48] = {
" ", " ", " ", " ", " ", " ", " ", " ",
" ", " ", " ", " ", " ", " ", " ", " ",
"◆", "▒", "␉", "␌", "␍", "␊", "°", "±",
"␤", "␋", "┘", "┐", "┌", "└", "┼", "⎺",
"⎻", "─", "⎼", "⎽", "├", "┤", "┴", "┬",
"│", "≤", "≥", "π", "≠", "£", "·", " "};

// const wchar_t *wtest = L"❤️";

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
        scrollok(stdscr, true);
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
        scrollok(stdscr, true);
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
        cursor_mode_t cursor_mode;
        keypad_mode_t keypad_mode;
        FILE *log;
        char G0;        /* G0 charset */
        char G1;        /* G1 charset */
        int charset;    /* 0 or 1 */
} term_ctx_t;


/* here we use ansi escape sequences to move the cursor LOL (quick test) */

void cur_lclear(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        if (parser->num_params == 0) {
                clrtoeol();
                return;
        }
        if (parser->num_params == 1) {
                switch(parser->params[0]) {
                case 0:
                        clrtoeol();
                        return;
                case 1:
                        /* not implemented */
                        return;
                case 2:
                        getyx(stdscr, y, x);
                        move(y, 0);
                        clrtoeol();
                        move(y, x);
                        return;
                }
        }

}


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
                if (parser->params[0] == 0) {
                        parser->params[0] == 1;
                }
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
                if (parser->params[0] == 0) {
                        parser->params[0] == 1;
                }
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
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (x + parser->params[0] <= maxx) {
                        x += parser->params[0];
                        move(y, x);
                        refresh();
                }
        }
}


void cursor_back(term_ctx_t *ctx)
{
        int x, y;
        getyx(stdscr, y, x);
        if (x > 0) {
                mvaddch(y, x-1, ' ');
                move(y, x-1);
                refresh();
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
                        if (parser->params[0] == 0) {
                                parser->params[0] = 1;
                        }
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

        if (ctx->charset == 0) {
                if (ctx->G0 == '0') {
                        if (ch >= 0x50 && ch <= 0x7F) {
                                waddstr(stdscr, DEC_special_as_utf8[ch - 0x50]); 
                                // waddstr(stdscr, wtest); 
                        } else {
                                waddch(stdscr, '?');
                        }
                } else {
                        waddch(stdscr, ch);
                }
        }
        
        if (ctx->charset == 1) {
                if (ctx->G1 == '0') {
                        if (ch >= 0x50 && ch <= 0x7F) {
                                waddstr(stdscr, DEC_special_as_utf8[ch - 0x50]); 
                                // waddwstr(stdscr, wtest); 
                        } else {
                                waddch(stdscr, '?');
                        }
                } else {
                        waddch(stdscr, ch);
                }
        }
        refresh();
}


void cur_newline()
{
        int maxx, maxy, x, y;

        clrtoeol();
        waddch(stdscr, '\n');
        refresh();
        return;
        getmaxyx(stdscr, maxy, maxx);
        getyx(stdscr, y, x);
        if (y + 1 <= maxy) {
                move(y + 1, 0);
        }
        refresh();
}


void cur_unsupported(term_ctx_t *ctx, vtparse_t *parser, unsigned int ch)
{
        fprintf(ctx->log, "Unsupported: [");
        if (parser->num_intermediate_chars == 1) {
                fprintf(ctx->log, "%c", parser->intermediate_chars[0]);
        }
        for (int i = 0; i < parser->num_params; i++) {
                fprintf(ctx->log, "%d", parser->params[i]);
                if (i < parser->num_params - 1) {
                        fprintf(ctx->log, ";");
                }
        }
        fprintf(ctx->log, "%c\n", ch);
        fflush(ctx->log);
}


void esc_unsupported(term_ctx_t *ctx, vtparse_t *parser, unsigned int ch)
{
        fprintf(ctx->log, "Unsupported ESC ");
        for (int i = 0; i < parser->num_intermediate_chars; i++) {
                fprintf(ctx->log, "%c", parser->intermediate_chars[i]);
        }
        fprintf(ctx->log, "%c\n", ch);
        fflush(ctx->log);
}


void vt100_normal_cursor_keys(term_ctx_t *ctx)
{
        ctx->cursor_mode = CURSOR_NORMAL;
}


void vt100_app_cursor_keys(term_ctx_t *ctx)
{
        ctx->cursor_mode = CURSOR_APP;
}


void vt100_normal_keypad(term_ctx_t *ctx)
{
        ctx->keypad_mode = KEYPAD_NORMAL;
}


void vt100_app_keypad(term_ctx_t *ctx)
{
        ctx->keypad_mode = KEYPAD_APP;
}


void vt100_set_G0(term_ctx_t *ctx, unsigned int ch)
{
        ctx->G0 = (char)ch;
        fprintf(ctx->log, "Switching G0 to %c\n", ch);
        fflush(ctx->log);
}


void vt100_set_G1(term_ctx_t *ctx, unsigned int ch)
{
        ctx->G1 = (char)ch;
        fprintf(ctx->log, "Switching G1 to %c\n", ch);
        fflush(ctx->log);
}


void vt100_shift_out(term_ctx_t *ctx)
{
        ctx->charset = 1;
}


void vt100_shift_in(term_ctx_t *ctx)
{
        ctx->charset = 0;
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
                case 14:
                        vt100_shift_out(parser->user_data);
                        break;
                case 15:
                        vt100_shift_in(parser->user_data);
                        break;
                case 8:
                case 127:
                        cursor_back(parser->user_data);
                        break;
                default:
                        break;
                }
                break;
        case VTPARSE_ACTION_ESC_DISPATCH:
                switch (ch) {
                case '0':
                case 'A':
                case 'B':
                        if (parser->num_intermediate_chars == 1) {
                                switch (parser->intermediate_chars[0]) {
                                case '(':
                                        vt100_set_G0(parser->user_data, ch);
                                        break;
                                case ')':
                                        vt100_set_G1(parser->user_data, ch);
                                        break;
                                default:
                                        esc_unsupported(parser->user_data, parser, ch);
                                        break;
                                }
                        }
                        break;
                case '=':
                        vt100_app_keypad(parser->user_data);
                        break;
                case '>':
                        vt100_normal_keypad(parser->user_data);
                        break;
                default:
                        esc_unsupported(parser->user_data, parser, ch);
                        break;
                }
                break;
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
                case 'K':
                        cur_lclear(parser->user_data, parser);
                        break;
                case 'J':
                        cur_clear(parser->user_data, parser);
                        break;
                case 'h':
                        if (parser->num_params == 1 && parser->params[0] == 2004) {
                                /* Enable bracket paste mode -
                                   currently not supported, command that is
                                   pasted, is encapsulated between
                                   ESC[200~ and ESC[201~ */
                        } else 
                        if (parser->num_intermediate_chars == 1 &&
                            parser->intermediate_chars[0] == '?') {
                                if (parser->num_params == 1 && parser->params[0] == 1) {
                                        vt100_app_cursor_keys(parser->user_data);
                                } else {
                                        cur_unsupported(parser->user_data, parser, ch);
                                }
                        } else {
                                cur_unsupported(parser->user_data, parser, ch);
                        }
                        break;
                case 'l':
                        if (parser->num_params == 1 && parser->params[0] == 2004) {
                                /* Disable bracket paste mode */
                                /* currently not supported */
                        } else 
                        if (parser->num_intermediate_chars == 1 &&
                            parser->intermediate_chars[0] == '?') {
                                if (parser->num_params == 1 && parser->params[0] == 1) {
                                        vt100_normal_cursor_keys(parser->user_data);
                                } else {
                                        cur_unsupported(parser->user_data, parser, ch);
                                }
                        } else {
                                cur_unsupported(parser->user_data, parser, ch);
                        }
                        break;
                default:
                        cur_unsupported(parser->user_data, parser, ch);
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
        setlocale(LC_ALL, "en_US.UTF-8");

        int MAXLINE = 2000;
        int fd1[2];
        int fd2[2];
        pid_t pid;
        char line[MAXLINE];

        vtparse_t parser;

        term_ctx_t ctx;
        ctx.log = fopen("log.txt", "w+");
        ctx.cursor_mode = CURSOR_NORMAL;
        ctx.keypad_mode = KEYPAD_NORMAL;
        ctx.charset = 0;
        ctx.G0 = 'B';
        ctx.G1 = 'B';

        term_config();

        parser.user_data = &ctx;
        vtparse_init(&parser, parser_callback);
        int master;
        pid = forkpty(&master, NULL, NULL, NULL);

        if (pid == -1) {
                printf("Error with forkpty");
                return -1;
        }

        if (!pid) {
                char* exec_argv[] = {"/bin/bash", NULL};
                char *env[] = {
                        "PATH=/usr/bin:/bin",
                        "HOME=/home/andreas",
                        "TERM=vt100",
                        "LC_ALL=C",
                        "USER=andreas",
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
                                        sprintf(keybuff, key_up_seq[ctx.cursor_mode]);
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_DOWN) {
                                        sprintf(keybuff, key_down_seq[ctx.cursor_mode]);
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_RIGHT) {
                                        sprintf(keybuff, key_right_seq[ctx.cursor_mode]);
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_LEFT) {
                                        sprintf(keybuff, key_left_seq[ctx.cursor_mode]);
                                        write(master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_BACKSPACE ||
                                    in == 0177) {
                                        sprintf(keybuff, "\177");
                                        write(master, keybuff, 1);
                                        continue;
                                }
                                if (in >= 1 && in <= 126) {
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
