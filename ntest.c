// #include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void clear(int what)
{
        printf("\033[%dJ", what);
        fflush(stdout);
}

void cursor_right(void)
{
        printf("\033[C");
        fflush(stdout);
}


void move(int y, int x)
{
        printf("\033[%d;%dH", y, x);
        fflush(stdout);
}


void scroll_down(void)
{
        printf("\033D");
        fflush(stdout);
}


void scroll_up(void)
{
        printf("\033M");
        fflush(stdout);
}


void scroll_reg(int start, int stop)
{
        printf("\033[%d;%dr", start, stop);
        fflush(stdout);
}

int main(void)
{
        clear(2);
        move(0, 0);
        for (int i = 0; i < 100; i++) {
                printf("haha ");
        }
        move(3, 20);
        for (int i = 0; i < 1000; i++) {
                //cursor_right();
                printf("h");
                fflush(stdout);
                //usleep(1000000);
        }

        sleep(3);
        clear(1);
        sleep(3);
        clear(2);
        move(0, 0);
        for (int i = 0; i < 100; i++) {
                printf("haha ");
        }
        move(3, 20);
        sleep(3);
        clear(0);
        sleep(3);
        move(26, 1);
        printf("Hallo");
        move(25, 1);
        printf("Hallo-scrollend");
        move(1,1);
        printf("First line");
        move(2,1);
        printf("Second Line");
        move(6,10);
        scroll_reg(2, 25);
        move(22,10);
        for (int i = 0; i < 100; i++) {
                scroll_down();
                sleep(1);
        }

        return 0;





/*
        initscr();
        refresh();

        WINDOW *w = newwin(20, 40, 10, 10);

        box(w, 0, 0);

        wmove(w, 0, 0);
        waddch(w, 'A');


        WINDOW *w2 = derwin(w, 18, 38, 1, 1);
        wrefresh(w2);
        box(w2, 0, 0);
        wmove(w2, 0, 0);
        waddch(w2, 'B');

        touchwin(w);
        wrefresh(w);

        refresh();
        doupdate();

        getch();

        endwin(); */
}
