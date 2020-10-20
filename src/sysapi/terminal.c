//
// Created by hujianzhe
//

#include "../../inc/sysapi/terminal.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if	defined(_WIN32) || defined(_WIN64)
static BOOL __set_tty_mode(HANDLE fd, DWORD mask, BOOL bval) {
	DWORD mode, mode_mask;
	if (!GetConsoleMode(fd, &mode))
		return FALSE;
	mode_mask = mode & mask;
	if (0 == mode_mask && !bval)
		return TRUE;
	if (mask == mode_mask && bval)
		return TRUE;
	if (bval)
		mode |= mask;
	else
		mode &= ~mask;
	return SetConsoleMode(fd, mode);
}
#else
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef	__linux__
#include <linux/input.h>

typedef struct DevFdSet_t {
    int* fd_ptr;
    size_t fd_cnt;
    int fd_max;
} DevFdSet_t;

static void free_dev_fd_set(DevFdSet_t* ds) {
    if (ds) {
        size_t i;
        for (i = 0; i < ds->fd_cnt; ++i) {
            close(ds->fd_ptr[i]);
        }
        free(ds->fd_ptr);
        ds->fd_ptr = NULL;
        ds->fd_cnt = 0;
        ds->fd_max = -1;
    }
}

static DevFdSet_t* scan_dev_fd_set(DevFdSet_t* ds) {
    DIR* dir;
    struct dirent* dir_item;

    dir = opendir("/dev/input");
    if (!dir) {
        return NULL;
    }
    ds->fd_ptr = NULL;
    ds->fd_cnt = 0;
    ds->fd_max = -1;
    while (dir_item = readdir(dir)) {
        int fd, i;
        long evs[EV_MAX/(sizeof(long)*8) + 1] = { 0 };
        char str_data[32];

        if (!strstr(dir_item->d_name, "event"))
            continue;
        i = sprintf(str_data, "/dev/input/%s", dir_item->d_name);
        if (i < 0)
            continue;
        str_data[i] = 0;
        fd = open(str_data, O_RDONLY);
        if (fd < 0)
            continue;
        /*
        ioctl(fd, EVIOCGNAME(sizeof(str_data) - 1), str_data);
        str_data[sizeof(str_data) - 1] = 0;
        */
        for (i = 0; i < sizeof(evs) / sizeof(evs[0]); ++i) {
            evs[i] = 0;
        }
        if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evs) < 0)
            continue;
        if (evs[EV_KEY / (sizeof(long) * 8)] & (1L << EV_KEY % (sizeof(long) * 8))) {
            int* fd_ptr = (int*)realloc(ds->fd_ptr, sizeof(ds->fd_ptr[0]) * (1 + ds->fd_cnt));
            if (!fd_ptr)
                break;
            ds->fd_ptr = fd_ptr;
            fd_ptr[ds->fd_cnt++] = fd;
            if (ds->fd_max < 0 || ds->fd_max < fd) {
                ds->fd_max = fd;
            }
        }
    }
    if (dir_item) {
        free_dev_fd_set(ds);
        return NULL;
    }
    return ds;
}
#endif
static tcflag_t __set_tty_flag(tcflag_t flag, tcflag_t mask, int bval) {
	tcflag_t flag_mask;
	flag_mask = flag & mask;
	if (0 == flag_mask && !bval)
		return flag;
	if (mask == flag_mask && bval)
		return flag;
	if (bval)
		flag |= mask;
	else
		flag &= ~mask;
	return flag;
}

static void __set_tty_no_canon_echo_isig(int ttyfd, struct termios* old, int min, int time) {
	struct termios newtc;
	tcgetattr(ttyfd, old);
	if (0 == (old->c_lflag & (ICANON|ECHO|ISIG)) &&
		old->c_cc[VMIN] == min && old->c_cc[VTIME] == time)
	{
		return;
	}
	newtc = *old;
	newtc.c_lflag &= ~(ICANON|ECHO|ISIG);
	newtc.c_cc[VMIN] = min;
	newtc.c_cc[VTIME] = time;
	tcsetattr(ttyfd, TCSANOW, &newtc);
}
#endif

FD_t terminalStdin(void) {
#if defined(_WIN32) || defined(_WIN64)
	HANDLE fd = GetStdHandle(STD_INPUT_HANDLE);
	return fd ? (FD_t)fd : (FD_t)INVALID_HANDLE_VALUE;
#else
	return isatty(STDIN_FILENO) ? STDIN_FILENO : INVALID_FD_HANDLE;
#endif
}

FD_t terminalStdout(void) {
#if defined(_WIN32) || defined(_WIN64)
	HANDLE fd = GetStdHandle(STD_OUTPUT_HANDLE);
	return fd ? (FD_t)fd : (FD_t)INVALID_HANDLE_VALUE;
#else
	return isatty(STDOUT_FILENO) ? STDOUT_FILENO : INVALID_FD_HANDLE;
#endif
}

FD_t terminalStderr(void) {
#if defined(_WIN32) || defined(_WIN64)
	HANDLE fd = GetStdHandle(STD_ERROR_HANDLE);
	return fd ? (FD_t)fd : (FD_t)INVALID_HANDLE_VALUE;
#else
	return isatty(STDERR_FILENO) ? STDERR_FILENO : INVALID_FD_HANDLE;
#endif
}

BOOL terminalFlushInput(FD_t fd) {
#if defined(_WIN32) || defined(_WIN64)
	return FlushConsoleInputBuffer((HANDLE)fd);
#else
	return tcflush(fd, TCIFLUSH) == 0;
#endif
}

char* terminalName(char* buf, size_t buflen) {
#if defined(_WIN32) || defined(_WIN64)
	DWORD res = GetConsoleTitleA(buf, buflen);
	if (res) {
		buf[buflen - 1] = 0;
		if (res < buflen) {
			buf[res] = 0;
		}
		return buf;
	}
	return NULL;
#else
	/*ctermid(buf);*/
	int res = ttyname_r(STDIN_FILENO, buf, buflen);
	if (res) {
		errno = res;
		return NULL;
	}
	else {
		buf[buflen - 1] = 0;
		return buf;
	}
#endif
}

int terminalKbhit(void) {
#if defined(_WIN32) || defined(_WIN64)
	return _kbhit();
#else
	int res = -1;
	struct termios old;
	__set_tty_no_canon_echo_isig(STDIN_FILENO, &old, 0, 0);
	ioctl(STDIN_FILENO, FIONREAD, &res);
	tcsetattr(STDIN_FILENO, TCSANOW, &old);
	return res > 0;
#endif
}

BOOL terminalEnableEcho(FD_t fd, BOOL bval) {
#if defined(_WIN32) || defined(_WIN64)
	return __set_tty_mode((HANDLE)fd, ENABLE_ECHO_INPUT, bval);
#else
	tcflag_t new_lflag;
	struct termios tm;
	if (tcgetattr(fd, &tm))
		return 0;
	new_lflag = __set_tty_flag(tm.c_lflag, ECHO, bval);
	if (new_lflag != tm.c_lflag) {
		tm.c_lflag = new_lflag;
		return tcsetattr(fd, TCSANOW, &tm) == 0;
	}
	return 1;
#endif
}

BOOL terminalEnableLineInput(FD_t fd, BOOL bval) {
#if defined(_WIN32) || defined(_WIN64)
	return __set_tty_mode((HANDLE)fd, ENABLE_LINE_INPUT, bval);
#else
	tcflag_t new_lflag;
	struct termios tm;
	if (tcgetattr(fd, &tm))
		return 0;
	new_lflag = __set_tty_flag(tm.c_lflag, ICANON | ECHO | IEXTEN, bval);
	if (new_lflag != tm.c_lflag) {
		tm.c_lflag = new_lflag;
		return tcsetattr(fd, TCSANOW, &tm) == 0;
	}
	return 1;
#endif
}

BOOL terminalEnableSignal(FD_t fd, BOOL bval) {
#if defined(_WIN32) || defined(_WIN64)
	return __set_tty_mode((HANDLE)fd, ENABLE_PROCESSED_INPUT, bval);
#else
	tcflag_t new_lflag;
	struct termios tm;
	if (tcgetattr(fd, &tm))
		return 0;
	new_lflag = __set_tty_flag(tm.c_lflag, ISIG, bval);
	if (new_lflag != tm.c_lflag) {
		tm.c_lflag = new_lflag;
		return tcsetattr(fd, TCSANOW, &tm) == 0;
	}
	return 1;
#endif
}

BOOL terminalGetPageSize(FD_t fd, int* x_col, int* y_row) {
#if defined(_WIN32) || defined(_WIN64)
	CONSOLE_SCREEN_BUFFER_INFO ws;
	if (!GetConsoleScreenBufferInfo((HANDLE)fd, &ws))
		return FALSE;
	*y_row = ws.srWindow.Right + 1;
	*x_col = ws.srWindow.Bottom + 1;
	return TRUE;
#else
	struct winsize ws;
	if (ioctl(fd, TIOCGWINSZ, (char*)&ws))
		return FALSE;
	*y_row = ws.ws_row;
	*x_col = ws.ws_col;
	return TRUE;
#endif
}

BOOL terminalSetCursorPos(FD_t fd, int x_col, int y_row) {
#if defined(_WIN32) || defined(_WIN64)
	COORD pos;
	pos.X = x_col;
	pos.Y = y_row;
	return SetConsoleCursorPosition((HANDLE)fd, pos);
#else
	char esc[11 + 11 + 4 + 1];
	int esc_len = sprintf(esc, "\033[%d;%dH", ++y_row, ++x_col);
	if (esc_len <= 0)
		return FALSE;
	esc[esc_len] = 0;
	return write(fd, esc, esc_len) == esc_len;
#endif
}

BOOL terminalShowCursor(FD_t fd, BOOL bval) {
#if defined(_WIN32) || defined(_WIN64)
	CONSOLE_CURSOR_INFO ci;
	ci.dwSize = 100;
	ci.bVisible = bval;
	return SetConsoleCursorInfo((HANDLE)fd, &ci);
#else
	if (bval) {
		char esc[] = "\033[?25h";
		return write(fd, esc, sizeof(esc) - 1) == sizeof(esc) - 1;
	}
	else {
		char esc[] = "\033[?25l";
		return write(fd, esc, sizeof(esc) - 1) == sizeof(esc) - 1;
	}
#endif
}

BOOL terminalClrscr(FD_t fd) {
#if defined(_WIN32) || defined(_WIN64)
	DWORD write_chars_num;
	COORD coord = { 0 };
	CONSOLE_SCREEN_BUFFER_INFO ws;
	if (!GetConsoleScreenBufferInfo((HANDLE)fd, &ws))
		return FALSE;
	write_chars_num = ws.dwSize.X * ws.dwSize.Y;
	return FillConsoleOutputCharacterA((HANDLE)fd, ' ', write_chars_num, coord, &write_chars_num);
#else
	char esc[] = "\033[2J";
	return write(fd, esc, sizeof(esc) - 1) == sizeof(esc) - 1;
#endif
}

BOOL terminalReadKey(FD_t fd, DevKeyEvent_t* e) {
#if defined(_WIN32) || defined(_WIN64)
	while (1) {
		DWORD n;
		INPUT_RECORD ir;
		if (!ReadConsoleInput((HANDLE)fd, &ir, 1, &n) || 0 == n)
			return FALSE;
		if (KEY_EVENT != ir.EventType)
			continue;
		if (ir.Event.KeyEvent.bKeyDown) {
			e->keydown = 1;
			e->charcode = ir.Event.KeyEvent.uChar.UnicodeChar;
			e->vkeycode = ir.Event.KeyEvent.wVirtualKeyCode;
			return TRUE;
		}
	}
#else
	int k = 0, res;
	res = read(fd, &k, sizeof(k));
	if (res > 1) {
		e->keydown = 1;
		e->charcode = 0;
		e->vkeycode = k;
		return 1;
	}
	else if (1 == res) {
		e->keydown = 1;
		e->charcode = k;
		e->vkeycode = 0;
		return 1;
	}
	return 0;
#endif
}

BOOL terminalReadKey2(FD_t fd, DevKeyEvent_t* e) {
#if defined(_WIN32) || defined(_WIN64)
	while (1) {
		DWORD n;
		INPUT_RECORD ir;
		if (!ReadConsoleInput((HANDLE)fd, &ir, 1, &n) || 0 == n)
			return FALSE;
		if (KEY_EVENT != ir.EventType)
			continue;
		e->keydown = ir.Event.KeyEvent.bKeyDown;
		e->charcode = ir.Event.KeyEvent.uChar.UnicodeChar;
		e->vkeycode = ir.Event.KeyEvent.wVirtualKeyCode;
		return TRUE;
	}
#elif __linux__
	/*
	unsigned char k;
	int res;
	long oldmode = 0;
	if (ioctl(fd, KDGKBMODE, &oldmode) < 0)
		return 0;
	if (ioctl(fd, KDSKBMODE, K_MEDIUMRAW) < 0)
		return 0;
	k = 0;
	res = read(fd, &k, sizeof(k));
	ioctl(fd, KDSKBMODE, oldmode);
	e->keydown = (0 == (k & 0x80) && res);
	e->charcode = k & 0x7f;
	e->vkeycode = k & 0x7f;
	return 1;
	*/
	DevFdSet_t ds;
	if (!scan_dev_fd_set(&ds) || 0 == ds.fd_cnt) {
		return 0;
	}
	while (1) {
		int i;
		fd_set rset;
		FD_ZERO(&rset);
		for (i = 0; i < ds.fd_cnt; ++i) {
			FD_SET(ds.fd_ptr[i], &rset);
		}
		i = select(ds.fd_max + 1, &rset, NULL, NULL, NULL);
		if (i < 0) {
			free_dev_fd_set(&ds);
			return 0;
		}
		else if (0 == i) {
			continue;
		}
		for (i = 0; i < ds.fd_cnt; ++i) {
			struct input_event input_ev;
			int len;
			if (!FD_ISSET(ds.fd_ptr[i], &rset))
				continue;
			len = read(ds.fd_ptr[i], &input_ev, sizeof(input_ev));
			if (len < 0)
				continue;
			else if (len != sizeof(input_ev))
				continue;
			else if (EV_KEY != input_ev.type)
				continue;
			free_dev_fd_set(&ds);
			e->keydown = (input_ev.value != 0);
			e->charcode = input_ev.code;
			e->vkeycode = input_ev.code;
			return 1;
		}
	}
#endif
	// TODO MAC
	e->keydown = 0;
	e->charcode = 0;
	e->vkeycode = 0;
	return FALSE;
}

#ifdef	__cplusplus
}
#endif
