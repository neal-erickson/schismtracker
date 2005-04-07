/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"

#include <SDL.h>

/* --------------------------------------------------------------------- */

/* ENSURE_MENU(optional return value)
 * will emit a warning and cause the function to return
 * if a menu is not active. */
#ifndef NDEBUG
# define ENSURE_MENU(...) G_STMT_START {\
        if ((status.dialog_type & DIALOG_MENU) == 0) {\
                fprintf(stderr, "%s called with no menu\n", __FUNCTION__);\
                return __VA_ARGS__;\
        }\
} G_STMT_END
#else
# define ENSURE_MENU(...)
#endif

/* --------------------------------------------------------------------- */
/* protomatypes */

static void main_menu_selected_cb(void);
static void file_menu_selected_cb(void);
static void playback_menu_selected_cb(void);
static void sample_menu_selected_cb(void);
static void instrument_menu_selected_cb(void);
static void settings_menu_selected_cb(void);

/* --------------------------------------------------------------------- */

struct menu {
        int x, y, w;
        const char *title;
        int num_items;  /* meh... */
        const char *items[14];  /* writing **items doesn't work here :( */
        int selected_item;      /* the highlighted item */
        int active_item;        /* "pressed" menu item, for submenus */
        void (*selected_cb) (void);     /* triggered by return key */
};

/* *INDENT-OFF* */
static struct menu main_menu = {
        x: 6, y: 11, w: 25, title: " Main Menu",
        num_items: 10, {
                "File Menu...",
                "Playback Menu...",
                "View Patterns        (F2)",
                "Sample Menu...",
                "Instrument Menu...",
                "View Orders/Panning (F11)",
                "View Variables      (F12)",
                "Message Editor (Shift-F9)",
                "Settings Menu...",
                "Help!                (F1)",
        }, selected_item: 0, active_item: -1,
        main_menu_selected_cb
};

static struct menu file_menu = {
        x: 25, y: 13, w: 22, title: "File Menu",
        num_items: 6, {
                "Load...           (F9)",
                "New...        (Ctrl-N)",
                "Save Current  (Ctrl-S)",
                "Save As...       (F10)",
                "Message Log  (Ctrl-F1)",
                "Quit          (Ctrl-Q)",
        }, selected_item: 0, active_item: -1,
        file_menu_selected_cb
};

static struct menu playback_menu = {
        x: 25, y: 13, w: 27, title: " Playback Menu",
        num_items: 7, {
                "Show Infopage          (F5)",
                "Play Song         (Ctrl-F5)",
                "Play Pattern           (F6)",
                "Play from Order  (Shift-F6)",
                "Play from Mark/Cursor  (F7)",
                "Stop                   (F8)",
                "Calculate Length   (Ctrl-P)",
        }, selected_item: 0, active_item: -1,
        playback_menu_selected_cb
};

static struct menu sample_menu = {
        x: 25, y: 20, w: 25, title: "Sample Menu",
        num_items: 3, {
                "Sample List          (F3)",
                "Sample Library  (Ctrl-F3)",
                "Reload Soundcard (Ctrl-G)",
        }, selected_item: 0, active_item: -1,
        sample_menu_selected_cb
};

static struct menu instrument_menu = {
        x: 20, y: 23, w: 29, title: "Instrument Menu",
        num_items: 2, {
                "Instrument List          (F4)",
                "Instrument Library  (Ctrl-F4)",
        }, selected_item: 0, active_item: -1,
        instrument_menu_selected_cb
};

static struct menu settings_menu = {
        x: 22, y: 32, w: 30, title: "Settings Menu",
	/* num_items is fiddled with when the menu is loaded (if there's no window manager,
	the toggle fullscreen item doesn't appear) */
        num_items: 2, {
                "Preferences         (Shift-F5)",
                "Palette Editor      (Ctrl-F12)",
                "Toggle Fullscreen  (Alt-Enter)",
        }, selected_item: 0, active_item: -1,
        settings_menu_selected_cb
};

/* *INDENT-ON* */

/* updated to whatever menu is currently active.
 * this generalises the key handling.
 * if status.dialog_type == DIALOG_SUBMENU, use current_menu[1]
 * else, use current_menu[0] */
static struct menu *current_menu[2] = { NULL, NULL };

/* --------------------------------------------------------------------- */

static void _draw_menu(struct menu *menu)
{
        int h = 6, n = menu->num_items;

        while (n--) {
                draw_box(2 + menu->x, 4 + menu->y + 3 * n,
                         5 + menu->x + menu->w, 6 + menu->y + 3 * n,
                         BOX_THIN | BOX_CORNER | (n ==
                                                  menu->
                                                  active_item ? BOX_INSET :
                                                  BOX_OUTSET));

                draw_text_len(menu->items[n], menu->w, 4 + menu->x,
                              5 + menu->y + 3 * n,
                              (n == menu->selected_item ? 3 : 0), 2);

                draw_char(0, 3 + menu->x, 5 + menu->y + 3 * n, 0, 2);
                draw_char(0, 4 + menu->x + menu->w, 5 + menu->y + 3 * n, 0,
                          2);

                h += 3;
        }

        draw_box(menu->x, menu->y, menu->x + menu->w + 7, menu->y + h - 1,
                 BOX_THICK | BOX_OUTER | BOX_FLAT_LIGHT);
        draw_box(menu->x + 1, menu->y + 1, menu->x + menu->w + 6,
                 menu->y + h - 2, BOX_THIN | BOX_OUTER | BOX_FLAT_DARK);
        draw_fill_chars(menu->x + 2, menu->y + 2, menu->x + menu->w + 5,
                        menu->y + 3, 2);
        draw_text(menu->title, menu->x + 6, menu->y + 2, 3, 2);
}

inline void menu_draw(void)
{
        ENSURE_MENU();

        _draw_menu(current_menu[0]);
        if (current_menu[1])
                _draw_menu(current_menu[1]);
}

/* --------------------------------------------------------------------- */

inline void menu_show(void)
{
        status.dialog_type = DIALOG_MAIN_MENU;
        current_menu[0] = &main_menu;

        status.flags |= NEED_UPDATE;
}

inline void menu_hide(void)
{
        ENSURE_MENU();

        status.dialog_type = DIALOG_NONE;

        /* "unpress" the menu items */
        current_menu[0]->active_item = -1;
        if (current_menu[1])
                current_menu[1]->active_item = -1;

        current_menu[0] = current_menu[1] = NULL;

        /* note! this does NOT redraw the screen; that's up to the caller.
         * the reason for this is that so many of the menu items cause a
         * page switch, and redrawing the current page followed by
         * redrawing a new page is redundant. */
}

/* --------------------------------------------------------------------- */

static inline void set_submenu(struct menu *menu)
{
        ENSURE_MENU();

        status.dialog_type = DIALOG_SUBMENU;
        main_menu.active_item = main_menu.selected_item;
        current_menu[1] = menu;

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* callbacks */

static void main_menu_selected_cb(void)
{
        switch (main_menu.selected_item) {
        case 0: /* file menu... */
                set_submenu(&file_menu);
                break;
        case 1: /* playback menu... */
                set_submenu(&playback_menu);
                break;
        case 2: /* view patterns */
                set_page(PAGE_PATTERN_EDITOR);
                break;
        case 3: /* sample menu... */
                set_submenu(&sample_menu);
                break;
        case 4: /* instrument menu... */
                set_submenu(&instrument_menu);
                break;
        case 5: /* view orders/panning */
                set_page(PAGE_ORDERLIST_PANNING);
                break;
        case 6: /* view variables */
                set_page(PAGE_SONG_VARIABLES);
                break;
        case 7: /* message editor */
                set_page(PAGE_MESSAGE);
                break;
        case 8: /* settings menu */
		if (status.flags & WM_AVAILABLE)
			settings_menu.num_items = 3;
                set_submenu(&settings_menu);
                break;
        case 9: /* help! */
                set_page(PAGE_HELP);
                break;
        }
}

static void file_menu_selected_cb(void)
{
        switch (file_menu.selected_item) {
        case 0: /* load... */
                set_page(PAGE_LOAD_MODULE);
                break;
	case 1: /* new... */
		new_song_dialog();
		break;
	case 2: /* save current */
		save_song_or_save_as();
		break;
	case 3: /* save as... */
		set_page(PAGE_SAVE_MODULE);
		break;
	case 4: /* message log */
		set_page(PAGE_LOG);
		break;
        case 5: /* quit */
                show_exit_prompt();
                break;
        }
}

static void playback_menu_selected_cb(void)
{
        switch (playback_menu.selected_item) {
        case 0: /* show infopage */
                if (song_get_mode() == MODE_STOPPED
		    || (song_get_mode() == MODE_SINGLE_STEP && status.current_page == PAGE_INFO))
                        song_start();
                set_page(PAGE_INFO);
                return;
        case 1: /* play song */
                song_start();
                break;
        case 2: /* play pattern */
                song_loop_pattern(get_current_pattern(), 0);
                break;
        case 3: /* play from order */
                song_start_at_order(get_current_order(), 0);
                break;
        case 4: /* play from mark/cursor */
                play_song_from_mark();
                break;
        case 5: /* stop */
                song_stop();
                break;
        case 6: /* calculate length */
                show_song_length();
                return;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

static void sample_menu_selected_cb(void)
{
        switch (sample_menu.selected_item) {
        case 0: /* sample list */
                set_page(PAGE_SAMPLE_LIST);
                return;
	case 1: /* sample library */
		break;
	case 2: /* reload soundcard */
		break;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

static void instrument_menu_selected_cb(void)
{
        switch (instrument_menu.selected_item) {
        case 0: /* instrument list */
                set_page(PAGE_INSTRUMENT_LIST);
                return;
	case 1: /* instrument library */
		break;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

static void settings_menu_selected_cb(void)
{
        switch (settings_menu.selected_item) {
        case 0: /* preferences page */
                set_page(PAGE_PREFERENCES);
                return;
	case 1: /* palette configuration */
                set_page(PAGE_PALETTE_EDITOR);
		return;
	case 2: /* toggle fullscreen */
		SDL_WM_ToggleFullScreen(screen);
		break;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

/* As long as there's a menu active, this function will return true. */
int menu_handle_key(SDL_keysym * k)
{
        struct menu *menu;
	
	if ((status.dialog_type & DIALOG_MENU) == 0)
		return 0;

	menu = (status.dialog_type == DIALOG_SUBMENU
		? current_menu[1] : current_menu[0]);

        switch (k->sym) {
        case SDLK_ESCAPE:
                current_menu[1] = NULL;
                if (status.dialog_type == DIALOG_SUBMENU) {
                        status.dialog_type = DIALOG_MAIN_MENU;
                        main_menu.active_item = -1;
                } else {
                        menu_hide();
                }
                break;
        case SDLK_UP:
                if (menu->selected_item > 0) {
                        menu->selected_item--;
                        break;
                }
                return 1;
        case SDLK_DOWN:
                if (menu->selected_item < menu->num_items - 1) {
                        menu->selected_item++;
                        break;
                }
                return 1;
                /* home/end are new here :) */
        case SDLK_HOME:
                menu->selected_item = 0;
                break;
        case SDLK_END:
                menu->selected_item = menu->num_items - 1;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                menu->selected_cb();
                return 1;
        default:
                return 1;
        }

        status.flags |= NEED_UPDATE;
	
	return 1;
}
