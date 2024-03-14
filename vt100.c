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
#include <sys/queue.h>
#include <pty.h>
#include <locale.h>
#include <pthread.h>

#include <curses.h>
#include <panel.h>

#include "vtparse.h"

#define UNUSED(x) { (void)x; }

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
        PANEL *panel;
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
        vtparse_t ansi_parser;
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
        box(ctx->pw, 0, 0);
        wmove(ctx->w, cy, cx);
}


void term_restore(void)
{
        endwin();
}


static void term_set_size(term_ctx_t *ctx)
{
        struct winsize {
                unsigned short ws_row;
                unsigned short ws_col;
                unsigned short ws_xpixel;
                unsigned short ws_ypixel;
        };

        struct winsize w;
        w.ws_row = ctx->dh;
        w.ws_col = ctx->dw;

        ioctl(ctx->master, TIOCSWINSZ, &w);
        term_frame_redraw(ctx);
}


static void handle_resizing()
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

        nodelay(stdscr, true);
}


void cur_lclear(term_ctx_t *ctx, vtparse_t *parser)
{
        int x, y;
        if (parser->num_params == 0) {
                wclrtoeol(ctx->w);
                term_frame_redraw(ctx);
                return;
        }
        if (parser->num_params == 1) {
                switch(parser->params[0]) {
                case 0:
                        wclrtoeol(ctx->w);
                        term_frame_redraw(ctx);
                        return;
                case 1:
                        getyx(ctx->w, y, x);
                        wmove(ctx->w, y, 0);
                        for (int i = 0; i < x-1; i++) {
                                waddch(ctx->w, ' ');
                        }
                        wmove(ctx->w, y, x);
                        return;
                case 2:
                        getyx(ctx->w, y, x);
                        wmove(ctx->w, y, 0);
                        wclrtoeol(ctx->w);
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
                }

        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (y - parser->params[0] >= 0) {
                        y -= parser->params[0];
                        wmove(ctx->w, y, x);
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
                }
        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (y + parser->params[0] <= ctx->dh) {
                        y += parser->params[0];
                        wmove(ctx->w, y, x);
                }
        }
}


void vt100_line_up(term_ctx_t *ctx)
{
        int x, y;
        getyx(ctx->w, y, x);

        if (y == ctx->scroll_start) {
                wscrl(ctx->w, -1);
                return;
        }

        /* we can move the cursor, but no scroll if
         * outside the scrolling range */
        if (y > 0) {
                wmove(ctx->w, y - 1, x);
                return;
        }
}


void vt100_line_down(term_ctx_t *ctx)
{
        int maxx; UNUSED(maxx);
        int maxy, y, x;

        getmaxyx(ctx->w, maxy, maxx);
        getyx(ctx->w, y, x);

        if (y == ctx->scroll_stop) {
                wscrl(ctx->w, 1);
                return;
        }

        /* we can move the cursor if outside of scroll region,
         * but we cannot scroll */
        if (y < maxy - 1) {
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
                        wmove(ctx->w, y, x);
                }
        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (x + parser->params[0] <= ctx->dw - 2) {
                        x += parser->params[0];
                        wmove(ctx->w, y, x);
                }
        }
}


void cursor_back(term_ctx_t *ctx)
{
        int x, y;
        getyx(ctx->w, y, x);
        if (x > ctx->mx) {
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
                        wmove(ctx->w, y, x);
                }
        } else {
                if (parser->params[0] == 0) {
                        parser->params[0] = 1;
                }
                if (x - parser->params[0] >= 0) {
                        x -= parser->params[0];
                        wmove(ctx->w, y, x);
                }
        }
}


void cur_home(term_ctx_t *ctx, vtparse_t *parser)
{
        if (parser->num_params == 0) {
                wmove(ctx->w, 0, 0);
                return;
        }

        int x, y;
        UNUSED(y);

        getyx(ctx->w, y, x);
        if (parser->num_params == 1) {
                if (parser->params[0] <= ctx->dh &&
                    parser->params[0] > 0) {
                        wmove(ctx->w, parser->params[0] - 1, x);
                }
                return;
        }

        if (parser->num_params == 2) {
                if (parser->params[0] <= ctx->dh &&
                    parser->params[0] > 0 &&
                    parser->params[1] <= ctx->dw &&
                    parser->params[1] > 0) {
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

        int cx, cy; UNUSED(cy);
        getyx(ctx->w, cy, cx);
        if (cx == 0) {
                term_frame_redraw(ctx);
        }

}


void cur_newline(term_ctx_t *ctx)
{
        int  y, _; UNUSED(_);

        vt100_line_down(ctx);
        getyx(ctx->w, y, _);
        wmove(ctx->w, y, 0);
        term_frame_redraw(ctx);
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
        int y, _; UNUSED(_);;

        getyx(ctx->w, y, _);
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
/*        wattron(ctx->pw, COLOR_PAIR(3));
        box(ctx->pw, 0, 0);
        wrefresh(ctx->pw);
        usleep(250000);
        wattroff(ctx->pw, COLOR_PAIR(3));
        box(ctx->pw, 0, 0);
        wrefresh(ctx->pw); */
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


void handle_input(term_ctx_t *ctx, int in)
{
        char keybuff[4];

        switch (in) {
        case KEY_UP:
                sprintf(keybuff, "%s", key_up_seq[ctx->cursor_mode]);
                write(ctx->master, keybuff, 3);
                break;
        case KEY_DOWN:
                sprintf(keybuff, "%s", key_down_seq[ctx->cursor_mode]);
                write(ctx->master, keybuff, 3);
                break;
        case KEY_RIGHT:
                sprintf(keybuff, "%s", key_right_seq[ctx->cursor_mode]);
                write(ctx->master, keybuff, 3);
                break;
        case KEY_LEFT:
                sprintf(keybuff, "%s", key_left_seq[ctx->cursor_mode]);
                write(ctx->master, keybuff, 3);
                break;
        case KEY_BACKSPACE:
                sprintf(keybuff, "\177");
                write(ctx->master, keybuff, 1);
                break;
        default:
                if (in >= 1 && in <= 126) {
                        write(ctx->master, &in, 1);
                        break;
                }
                if (in >= 256 && in < 256*256 - 1) {
                        write(ctx->master, &in, 2);
                        break;
                }
                if (in > 256*256 && in < 256*256*256 - 1) {
                        write(ctx->master, &in, 3);
                        break;
                }
                break;
        }
}


int handle_output(term_ctx_t *ctx)
{
        int rc;
        char buffer[128];
        struct timeval timeout;

        timeout.tv_sec = 0;
        timeout.tv_usec = 100;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(ctx->master, &fds);

        /* look if something is there on child's stdout */
        rc = select(ctx->master + 1, &fds, NULL, NULL, &timeout);
        if (rc <= 0) {
                return 0;
        }

        rc = read(ctx->master, buffer, sizeof(buffer));
        if (rc > 0) {
                /* handle by ANSI Escape Sequence State Machine */
                vtparse(&ctx->ansi_parser, (unsigned char *)buffer, rc);
                return 0;
        }

        /* read returns 0 despite select was > 0, this means,
           the socket has closed and child has terminated */
        return -1;
}


term_ctx_t *new_term(int x, int y, int cols, int rows)
{
        term_ctx_t *t = malloc(sizeof(term_ctx_t));
        if (!t) {
                return NULL;
        }

        t->log = fopen("log.txt", "w+");

        t->cursor_mode = CURSOR_NORMAL;
        t->keypad_mode = KEYPAD_NORMAL;
        t->charset = 0;
        t->G0 = 'B';
        t->G1 = 'B';

        t->ww = cols + 2;
        t->wh = rows + 2;
        t->pw = newwin(t->wh, t->ww, y, x);
        t->panel = new_panel(t->pw);

        t->dh = t->wh - 2;
        t->dw = t->ww - 2;
        t->wx = x;
        t->wy = y;
        t->mx = 1;
        t->my = 1;
        t->w = derwin(t->pw, t->dh, t->dw+1, t->my, t->mx);
        t->scroll_start = 0;
        t->scroll_stop = t->dh - 1;

        vtparse_init(&t->ansi_parser, parser_callback);
        t->ansi_parser.user_data = t;

        wattron(t->w, COLOR_PAIR(2));
        box(t->pw, 0, 0);
        scrollok(t->w, true);

        return t;
}


pid_t term_process(term_ctx_t *ctx, char **argv, char **env)
{
        pid_t pid = forkpty(&ctx->master, NULL, NULL, NULL);

        if (pid == -1) {
                perror("forkpty");
                return -1;
        }

        if (pid == 0) {
                /* replace child process */
                execve(argv[0], argv, env);
                /* only reaches here in case of error */
                perror("CHILD: execvp");
                return EXIT_FAILURE;
        }

        /* adjust pseudo terminal size */
        term_set_size(ctx);

        return pid;
}


CIRCLEQ_HEAD(termlist, termlist_entry) termlist_head;

// struct termlist *termlist_headp;

typedef struct termlist_entry {
        CIRCLEQ_ENTRY(termlist_entry) entries;
        term_ctx_t *term_ctx;
        char *term_name;
        pid_t term_proc;
        bool is_active;
        uint64_t id;
        char **argv;
        char **env;
        pthread_t manager_thread;
        bool kill_me;
} termlist_entry_t;


uint64_t terminal_manager_create(char *name, int x, int y, int cols, int rows)
{

        termlist_entry_t *tle;

        tle = malloc(sizeof(termlist_entry_t));
        if (!tle) {
                perror("out of memory!");
                exit(-1);
        }

        /* make all current terminals inactive */
        uint64_t max_id = 0;

        termlist_entry_t *p;
        for (p = termlist_head.cqh_first; p != (void *)&termlist_head;
             p = p->entries.cqe_next) {
                p->is_active = false;
                if (p->id > max_id) {
                        max_id = p->id;
                }
        }

        tle->id = max_id + 1;
        tle->is_active = true;
        tle->manager_thread = -1;
        /* create a new virtual terminal window */
        tle->term_ctx = new_term(x, y, cols, rows);
        if (!tle->term_ctx) {
                perror("new_term");
                return 0;
        }
        tle->kill_me = false;

        CIRCLEQ_INSERT_HEAD(&termlist_head, tle, entries);

        return tle->id;
}


termlist_entry_t *terminal_manager_id_to_entry(uint64_t id)
{
        bool found = false;
        termlist_entry_t *p;

        for (p = termlist_head.cqh_first; p != (void *)&termlist_head;
             p = p->entries.cqe_next) {
                if (p->id == id) {
                        found = true;
                        break;
                }
        }

        if (!found) {
                p = NULL;
        }
        return p;
}


void *terminal_manager_thread(void *i)
{
        uint64_t id = (uint64_t)i;

        termlist_entry_t *e = terminal_manager_id_to_entry(id);

        if (!e) {
                /* something is wrong here */
                return NULL;
        }

        term_ctx_t *ctx = e->term_ctx;
        pid_t pid = term_process(ctx, e->argv, e->env);

        (void)pid;
        handle_resizing(e->term_ctx);

        while (!e->kill_me) {
                /* keep main process of thread idle */
        }

/*        if (waitpid(e->term_proc, NULL, 0) == -1) {
                perror("PARENT: waitpid");
                exit(-1);
        } */

        printf("Thread exited");
        return NULL;
}


pthread_t terminal_manager_run(uint64_t term_id, char **argv, char **env)
{
        termlist_entry_t *e = terminal_manager_id_to_entry(term_id);
        if (!e) {
                perror("Invalid terminal id");
                exit(-1);
        }

        e->argv = argv;
        e->env = env;

        pthread_create(&e->manager_thread, NULL,
                       terminal_manager_thread, (void *)term_id);

        return e->manager_thread;
}


uint64_t terminal_manager_get_active(void)
{
        termlist_entry_t *p;
        for (p = termlist_head.cqh_first; p != (void *)&termlist_head;
             p = p->entries.cqe_next) {
                if (p->is_active) {
                        return p->id;
                }
        }
        return 0;
}


void terminal_manager_join_threads(void)
{
        termlist_entry_t *p;
        for (p = termlist_head.cqh_first; p != (void *)&termlist_head;
             p = p->entries.cqe_next) {
                if (p->manager_thread != -1) {
                        pthread_join(p->manager_thread, NULL);
                }
        }
}


int terminal_manager_update_views(void)
{
        term_ctx_t *active_ctx = NULL;

        termlist_entry_t *p;
        for (p = termlist_head.cqh_first; p != (void *)&termlist_head;
             p = p->entries.cqe_next) {
                if (p->is_active) {
                        active_ctx = p->term_ctx;
                }
                if (handle_output(p->term_ctx) < 0) {
                        /* child exited, remove this window from the list,
                         * and signal thread to exit */
                        return 1;
                        // break;
                }
        }

        if (active_ctx) {
                term_frame_redraw(active_ctx);
        }
        update_panels();
        doupdate();
        return 0;
}


void terminal_manager_terminate(termlist_entry_t *e)
{
        del_panel(e->term_ctx->panel);
        delwin(e->term_ctx->w);
        delwin(e->term_ctx->pw);
        CIRCLEQ_REMOVE(&termlist_head, e, entries);
        e->kill_me = true;
}


/* execute process in virtual terminal with given
   environment */
char *exec_argv[] = {"/bin/bash", NULL};
char *env[] = {
        "PATH=/usr/bin:/bin",
        "HOME=/home/andreas",
        "TERM=vt100",
        "LC_ALL=C",
        "USER=andreas",
        0
};


termlist_entry_t *new_command_win(int x, int y)
{
        uint64_t id = terminal_manager_create(
                        "Terminal", x, y, 80, 25);
        if (id == 0) {
                /* error, could not create terminal */
                perror("Out of memory");
                exit(-1);
        }
        terminal_manager_run(id, exec_argv, env);

        return terminal_manager_id_to_entry(id);
}


termlist_entry_t *terminal_manager_next(termlist_entry_t *active)
{
        termlist_entry_t *next = active->entries.cqe_next;
        if (!next->term_ctx) {
                /* the root element of the circular list is unused */
                /* jump to the next */
                next = next->entries.cqe_next;
        }
        active->is_active = false;
        next->is_active = true;
        top_panel(next->term_ctx->panel);
        return next;
}


int main(int argc, char *argv[])
{
        /* Initialice locale and ncurses */
        setlocale(LC_ALL, "en_US.UTF-8");
        term_config();

        CIRCLEQ_INIT(&termlist_head);

        uint64_t id = terminal_manager_create("Terminal", 1, 1, 80, 25);
        if (id == 0) {
                /* error, could not create terminal */
                perror("Out of memory");
                exit(-1);
        }
        terminal_manager_run(id, exec_argv, env);

        /* here in the main thread and process, we
         * only fetch stdin via ncurses and check if we
         * have someting to manage */

        termlist_entry_t *active = terminal_manager_id_to_entry(id);
        handle_resizing();

        doupdate();
        bool cmd_mode = false;
        int xi = 2, yi = 2;
        while (true) {
                int res = terminal_manager_update_views();
                if (res) {
                        /* some child has terminated */
                        termlist_entry_t *active_old;
                        active_old = active;
                        active = terminal_manager_next(active);
                        if (active_old == active) {
                                /* we are finished */
                                break;
                        }
                        /* remove window, panel and kill pid */
                        terminal_manager_terminate(active_old);
                }
                int in = getch();
                if (in != ERR) {
                        if (in == KEY_RESIZE) {
                                handle_resizing();
                                continue;
                        }
                        if (cmd_mode) {
                                switch (in) {
                                case 'w':
                                        xi += 2;
                                        yi += 2;
                                        active = new_command_win(xi, yi);
                                        break;
                                case 'n':
                                        active = terminal_manager_next(active);
                                        break;
                                }
                                cmd_mode = false;
                                continue;
                        }

                        if (in == ('w' & 0x1F)) {
                                cmd_mode = true;
                                continue;
                        }
                        handle_input(active->term_ctx, in);
                }
        }

        terminal_manager_join_threads();

        return EXIT_SUCCESS;
}
