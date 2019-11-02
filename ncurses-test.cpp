#include <ncurses.h>
#define CLICKER_LEFT 339
#define CLICKER_RIGHT 338


int main()
{
	initscr();			/* Start curses mode 		  */
	printw("Hello World !!!");	/* Print Hello World		  */
	refresh();			/* Print it on to the real screen */
  printw("Hello World 2!!!");
  refresh();

  keypad(stdscr, TRUE);
  mousemask(ALL_MOUSE_EVENTS, NULL);

  bool found = false;

  while (!found)
  {
    MEVENT event;

    int ch = getch();
    if (ch != -1) {
        switch (ch) {
          case CLICKER_LEFT:
            printw("LEFT CLICKER\n");
            break;
          case CLICKER_RIGHT:
            printw("RIGHT CLICKER\n");
            break;
          refresh();
        }
      }
    }


  endwin();			/* End curses mode		  */

	return 0;
}
