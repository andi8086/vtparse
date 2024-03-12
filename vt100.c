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


typedef struct {
        cursor_mode_t cursor_mode;
        keypad_mode_t keypad_mode;
        FILE *log;
        char G0;        /* G0 charset */
        char G1;        /* G1 charset */
        int charset;    /* 0 or 1 */
        WINDOW *pw;     /* pwin */
        WINDOW *w;      /* derwin */
        int wx;
        int wy;
        int ww;
        int wh;
        int mx; /* margin */
        int my; /* margin */
        int dw; /* display width */
        int dh; /* display height */
        int master;
        int scroll_start;
        int scroll_stop;
} term_ctx_t;


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


void term_frame_redraw(term_ctx_t *ctx)
{
        int cx, cy;
        getyx(ctx->w, cy, cx);
        wrefresh(ctx->pw);
        wrefresh(ctx->w);
        box(ctx->pw, 0, 0);
        char coords_msg[64];

        snprintf(coords_msg, 63, "x = %d, y = %d, scroll_start = %d, scroll_stop = %d    ",
                             cx + 1, cy + 1, ctx->scroll_start + 1, ctx->scroll_stop + 1);
        mvwaddstr(stdscr, ctx->wy + ctx->wh + 2, 2, "The power of VT100 :-)");
        mvwaddstr(stdscr, ctx->wy + ctx->wh + 3, 2, coords_msg);
        refresh();
        doupdate();
        wmove(ctx->w, cy, cx);
}


void term_restore(void)
{
        endwin();
}


static void handle_resizing(term_ctx_t *ctx)
{
        endwin();

        struct winsize {
                unsigned short ws_row;
                unsigned short ws_col;
                unsigned short ws_xpixel;
                unsigned short ws_ypixel;
        };

        struct winsize w;
        w.ws_row = ctx->dh;
        w.ws_col = ctx->dw;

        initscr();
        keypad(stdscr, true);
        cbreak();
        noecho();
        scrollok(stdscr, true);
        ioctl(ctx->master, TIOCSWINSZ, &w);
        term_frame_redraw(ctx);
}


void term_config(void)
{
        if (!isatty(STDIN_FILENO)) {
                perror("Not a tty");
                exit(-1);
        }

        initscr();
        atexit(term_restore);
        keypad(stdscr, true);
        cbreak();
        noecho();
        move(0, 0);
        curs_set(1);
        scrollok(stdscr, true);
        refresh();

        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK); 
        init_pair(3, COLOR_RED, COLOR_BLACK); 
}


void cur_lclear(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        if (parser->num_params == 0) {
                wclrtoeol(ctx->w);
                touchwin(ctx->pw); 
                wrefresh(ctx->w);
                term_frame_redraw(ctx);
                return;
        }
        if (parser->num_params == 1) {
                switch(parser->params[0]) {
                case 0:
                        wclrtoeol(ctx->w);
                        touchwin(ctx->pw); 
                        term_frame_redraw(ctx);
                        return;
                case 1:
                        getyx(ctx->w, y, x);
                        wmove(ctx->w, y, 0);
                        for (int i = 0; i < x-1; i++) {
                                waddch(ctx->w, ' '); 
                        }
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        wmove(ctx->w, y, x);
                        return;
                case 2:
                        getyx(ctx->w, y, x);
                        wmove(ctx->w, y, 0);
                        wclrtoeol(ctx->w);
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        term_frame_redraw(ctx);
                        wmove(ctx->w, y, x);
                        return;
                }
        }

}


void cur_clear(term_ctx_t *ctx, vtparse_t *parser)
{
        if (parser->num_params == 0) {
                wclrtobot(ctx->w);
                term_frame_redraw(ctx);
                return;
        }
        if (parser->num_params == 1) {
                switch(parser->params[0]) {
                case 0:
                        wclrtobot(ctx->w);
                        term_frame_redraw(ctx);
                        return;
                case 1:
                        /* not implemented */
                        return;
                case 2:
                        wclear(ctx->w);
                        term_frame_redraw(ctx);
                        return;
                }
        }
}


void cur_attr(term_ctx_t *ctx, vtparse_t *parser)
{
        if (parser->num_params == 0) {
                wattrset(ctx->w, WA_NORMAL);
                return;
        }
        if (parser->num_params == 1) {
                switch(parser->params[0]) {
                case 0:
                        wattrset(ctx->w, WA_NORMAL);
                        return;
                case 1:
                        wattrset(ctx->w, WA_BOLD);
                        return;
                case 2:
                        wattrset(ctx->w, WA_DIM);
                        return;
                case 4:
                        wattrset(ctx->w, WA_UNDERLINE);
                        return;
                case 5:
                        wattrset(ctx->w, WA_BLINK);
                        return;
                case 7:
                        wattrset(ctx->w, WA_REVERSE);
                        return;
                default:
                        return;
                }
        }
}


void cur_up(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        getyx(ctx->w, y, x);
        if (parser->num_params == 0) {
                if (y > 0) {
                        y--;
                        wmove(ctx->w, y, x);
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                }

        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (y - parser->params[0] >= 0) {
                        y -= parser->params[0];
                        wmove(ctx->w, y, x);
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                }
        }
}


void cur_down(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        getyx(ctx->w, y, x);
        if (parser->num_params == 0) {
                if (y < ctx->dh) {
                        y++;
                        wmove(ctx->w, y, x);
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                }
        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (y + parser->params[0] <= ctx->dh) {
                        y += parser->params[0];
                        wmove(ctx->w, y, x);
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                }
        }
}


void vt100_line_up(term_ctx_t *ctx)
{
        int x, y;
        getyx(ctx->w, y, x);

        if (y == ctx->scroll_start) {
                wscrl(ctx->w, -1);
                touchwin(ctx->pw);
                wrefresh(ctx->w);
                return;
        }

        /* we can move the cursor, but no scroll if
         * outside the scrolling range */
        if (y > 0) {
                touchwin(ctx->pw);
                wrefresh(ctx->w);
                wmove(ctx->w, y - 1, x);
                return;
        }
}


void vt100_line_down(term_ctx_t *ctx)
{
        touchwin(ctx->pw);
        wrefresh(ctx->w);

        int maxx, maxy, y, x;

        getmaxyx(ctx->w, maxy, maxx);
        getyx(ctx->w, y, x);

        if (y == ctx->scroll_stop) {
                wscrl(ctx->w, 1);
                touchwin(ctx->pw);
                wrefresh(ctx->w);
                return;
        }

        /* we can move the cursor if outside of scroll region,
         * but we cannot scroll */
        if (y < maxy - 1) {
                touchwin(ctx->pw);
                wrefresh(ctx->w);
                wmove(ctx->w, y + 1, x);
                return;
        }
}


void cur_right(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        getyx(ctx->w, y, x);
        if (parser->num_params == 0) {
                if (x < ctx->dw - 2) {
                        x++;
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        wmove(ctx->w, y, x);
                }
        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (x + parser->params[0] <= ctx->dw - 2) {
                        x += parser->params[0];
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        wmove(ctx->w, y, x);
                }
        }
}


void cursor_back(term_ctx_t *ctx)
{
        int x, y;
        getyx(ctx->w, y, x);
        if (x > ctx->mx) {
                touchwin(ctx->pw);
                wrefresh(ctx->w);
                wmove(ctx->w, y, x-1);
        }
}


void cur_left(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        getyx(ctx->w, y, x);
        if (parser->num_params == 0) {
                if (x > 0) {
                        x--;
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        wmove(ctx->w, y, x);
                }
        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (x - parser->params[0] >= 0) {
                        x -= parser->params[0];
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        wmove(ctx->w, y, x);
                }
        }
}


void cur_home(term_ctx_t *ctx, vtparse_t *parser)
{
        if (parser->num_params == 0) {
                touchwin(ctx->pw);
                wrefresh(ctx->w);
                wmove(ctx->w, 0, 0);
                return;
        }

        int x, y;
        getyx(ctx->w, y, x);
        if (parser->num_params == 1) {
                if (parser->params[0] <= ctx->dh &&
                    parser->params[0] > 0) {
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        wmove(ctx->w, parser->params[0] - 1, x);
                }
                return;
        }

        if (parser->num_params == 2) {
                if (parser->params[0] <= ctx->dh &&
                    parser->params[0] > 0 &&
                    parser->params[1] <= ctx->dw &&
                    parser->params[1] > 0) {
                        touchwin(ctx->pw);
                        wrefresh(ctx->w);
                        wmove(ctx->w, parser->params[0] - 1, parser->params[1] - 1);
                }
                return;
        }
}


void term_put(term_ctx_t *ctx, vtparse_t *parser, unsigned int ch)
{
        wattron(ctx->w, COLOR_PAIR(2));
        if (ctx->charset == 0) {
                if (ctx->G0 == '0') {
                        if (ch >= 0x50 && ch <= 0x7F) {
                                waddstr(ctx->w, DEC_special_as_utf8[ch - 0x50]);
                        } else {
                                waddch(ctx->w, '?');
                        }
                } else {
                        waddch(ctx->w, ch);
                }
        }

        if (ctx->charset == 1) {
                if (ctx->G1 == '0') {
                        if (ch >= 0x50 && ch <= 0x7F) {
                                waddstr(ctx->w, DEC_special_as_utf8[ch - 0x50]);
                        } else {
                                waddch(ctx->w, '?');
                        }
                } else {
                        waddch(ctx->w, ch);
                }
        }
        touchwin(ctx->pw);
        wrefresh(ctx->w);
       
        int cx, cy;
        getyx(ctx->w, cy, cx);
        if (cx == 0) {
                term_frame_redraw(ctx);
        }

}


void cur_newline(term_ctx_t *ctx)
{
        int  y, _;

        vt100_line_down(ctx);
        getyx(ctx->w, y, _);
        term_frame_redraw(ctx); 
        touchwin(ctx->pw);
        wrefresh(ctx->w);
        wmove(ctx->w, y, 0);

        return;
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


void vt100_set_scroll_region(term_ctx_t *ctx, vtparse_t *parser)
{
        int y, x;

        getmaxyx(ctx->w, y, x);
        /* in NCURSES, we are zero-based, and the name is misleading,
         * it returns the number of columns and rows... so we have
         * to subtract one to get the max coordinates */
        y--;
        x--;

        if (parser->num_params == 0) {
                /* reset region to whole window */
               wsetscrreg(ctx->w, 0, y);
               ctx->scroll_start = 0;
               ctx->scroll_stop = y;
               return;
        }

        int start = parser->params[0];
        int stop = parser->params[1];

        /* now the ANSI parameters are 1-based and curses are 0-based */
        if (start < 1 || start > y || stop > y + 1 || stop < 2 ||
            stop <= start) {
                /* invalid parameter */
                return;
        }
        ctx->scroll_start = start - 1;
        ctx->scroll_stop = stop - 1;
        wsetscrreg(ctx->w, start - 1, stop - 1);
        term_frame_redraw(ctx);
}


void vt100_cursor_col_zero(term_ctx_t *ctx)
{
        int y, _;

        getyx(ctx->w, y, _);
        // touchwin(ctx->pw);
        // wrefresh(ctx->w);
        wmove(ctx->w, y, 0);
}


void vt100_next_tab(term_ctx_t *ctx)
{
        int y, x;

        getyx(ctx->w, y, x);
        x = ((x / 8) + 1) * 8;
        wmove(ctx->w, y, x);

}


void vt100_bell(term_ctx_t *ctx)
{
        wattron(ctx->pw, COLOR_PAIR(3));
        box(ctx->pw, 0, 0);
        wrefresh(ctx->pw);
        usleep(250000);
        wattroff(ctx->pw, COLOR_PAIR(3));
        box(ctx->pw, 0, 0);
        wrefresh(ctx->pw);
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
                        vt100_bell(parser->user_data);
                        break;
                case 9:
                        vt100_next_tab(parser->user_data);
                        break;
                case 10:
                        /* new line */
                        cur_newline(parser->user_data);
                        break;
                case 13:
                        vt100_cursor_col_zero(parser->user_data);
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
                case 'M':
                        vt100_line_up(parser->user_data);
                        break;
                case 'D':
                        vt100_line_down(parser->user_data);
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
                case 'r':
                        if (parser->num_params == 2 || parser->num_params == 0) {
                                vt100_set_scroll_region(parser->user_data, parser);
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


void cursor_fix(WINDOW *w)
{
        int cx, cy;
        getyx(w, cy, cx);
        wmove(w, cy, cx);
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

        ctx.ww = 82;
        ctx.wh = 27;
        ctx.pw = newwin(ctx.wh, ctx.ww, 1, 1);
        ctx.dh = ctx.wh - 2;
        ctx.dw = ctx.ww - 2;
        ctx.wx = 1;
        ctx.wy = 1;
        ctx.mx = 1;
        ctx.my = 1;
        ctx.w = derwin(ctx.pw, ctx.dh, ctx.dw+1, ctx.mx, ctx.my);
        ctx.scroll_start = 0;
        ctx.scroll_stop = ctx.dh - 1;
        wattron(ctx.w, COLOR_PAIR(2));

        box(ctx.pw, 0, 0);
        touchwin(ctx.pw);
        wrefresh(ctx.w);
        scrollok(ctx.pw, true);
        scrollok(ctx.w, true);
        term_frame_redraw(&ctx);

        parser.user_data = &ctx;
        vtparse_init(&parser, parser_callback);
        pid = forkpty(&ctx.master, NULL, NULL, NULL);


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
                handle_resizing(&ctx);

                nodelay(stdscr, true);
                while (1) {
                        int rcv;

                        char buffer[128];
                        touchwin(ctx.pw);
                        wrefresh(ctx.w);
                        refresh();
                        doupdate();

                        /* look if something is there on child's stdout */
                        struct timeval timeout;
                        timeout.tv_sec = 0;
                        timeout.tv_usec = 100;
                        fd_set fds;
                        FD_ZERO(&fds);
                        FD_SET(ctx.master, &fds);

                        int rc = select(ctx.master + 1, &fds, NULL, NULL, &timeout);
                        if (rc > 0) {
                                rcv = read(ctx.master, buffer, sizeof(buffer));
                                if (rcv > 0) {
                                        vtparse(&parser, (unsigned char *)buffer, rcv);
                                }
                                touchwin(ctx.pw);
                                wrefresh(ctx.w);
                                cursor_fix(ctx.w);
                                doupdate();
                        }
                        int in = getch();
                        if (in != ERR) {
                                char keybuff[4];
                                if (in == KEY_RESIZE) {
                                        handle_resizing(&ctx);
                                        continue;
                                }
                                if (in == KEY_UP) {
                                        sprintf(keybuff, key_up_seq[ctx.cursor_mode]);
                                        write(ctx.master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_DOWN) {
                                        sprintf(keybuff, key_down_seq[ctx.cursor_mode]);
                                        write(ctx.master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_RIGHT) {
                                        sprintf(keybuff, key_right_seq[ctx.cursor_mode]);
                                        write(ctx.master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_LEFT) {
                                        sprintf(keybuff, key_left_seq[ctx.cursor_mode]);
                                        write(ctx.master, keybuff, 3);
                                        continue;
                                }
                                if (in == KEY_BACKSPACE ||
                                    in == 0177) {
                                        sprintf(keybuff, "\177");
                                        write(ctx.master, keybuff, 1);
                                        continue;
                                }
                                if (in >= 1 && in <= 126) {
                                        write(ctx.master, &in, 1);
                                        continue;
                                }
                                if (in >= 256 && in < 256*256 - 1) {
                                        write(ctx.master, &in, 2);
                                        continue;
                                }
                                if (in > 256*256 && in < 256*256*256 - 1) {
                                        write(ctx.master, &in, 3);
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
