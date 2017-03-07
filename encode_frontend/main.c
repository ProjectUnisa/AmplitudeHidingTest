/*
*      Command line frontend program
*
*      Copyright (c) 1999 Mark Taylor
*                    2000 Takehiro TOMINAGA
*                    2010-2011 Robert Hegemann
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
*/

/* $Id: main.c,v 1.127 2011/10/02 14:52:20 robert Exp $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char   *strchr(), *strrchr();
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef __sun__
/* woraround for SunOS 4.x, it has SEEK_* defined here */
#include <unistd.h>
#endif

#ifdef __OS2__
#include <os2.h>
#define PRTYC_IDLE 1
#define PRTYC_REGULAR 2
#define PRTYD_MINIMUM -31
#define PRTYD_MAXIMUM 31
#endif

#if defined(_WIN32)
# include <windows.h>
#endif


/*
main.c is example code for how to use libmp3lame.a.  To use this library,
you only need the library and lame.h.  All other .h files are private
to the library.
*/
#include "lame.h"

#include "console.h"
#include "main.h"

/* PLL 14/04/2000 */
#if macintosh
#include <console.h>
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>


#define GOS_DIVISION 3
#define SAMPLES_PER_GOS_SECTION 200
#define SAMPLES_PER_GOS (GOS_DIVISION * SAMPLES_PER_GOS_SECTION)
#define LEFT 0
#define R 1


/************************************************************************
*
* main
*
* PURPOSE:  MPEG-1,2 Layer III encoder with GPSYCHO
* psychoacoustic model.
*
************************************************************************/
void
print_trailing_info(lame_global_flags * gf);
int
write_xing_frame(lame_global_flags * gf, FILE * outf, size_t offset);
int
write_id3v1_tag(lame_t gf, FILE * outf);

#if defined( _WIN32 ) && !defined(__MINGW32__)
static void
set_process_affinity()
{
#if 0
	/* rh 061207
	the following fix seems to be a workaround for a problem in the
	parent process calling LAME. It would be better to fix the broken
	application => code disabled.
	*/
#if defined(_WIN32)
	/* set affinity back to all CPUs.  Fix for EAC/lame on SMP systems from
	"Todd Richmond" <todd.richmond@openwave.com> */
	typedef BOOL(WINAPI * SPAMFunc) (HANDLE, DWORD_PTR);
	SPAMFunc func;
	SYSTEM_INFO si;

	if ((func = (SPAMFunc)GetProcAddress(GetModuleHandleW(L"KERNEL32.DLL"),
		"SetProcessAffinityMask")) != NULL) {
		GetSystemInfo(&si);
		func(GetCurrentProcess(), si.dwActiveProcessorMask);
	}
#endif
#endif
}
#endif

#if defined(WIN32)

/**
*  Long Filename support for the WIN32 platform
*
*/

void
dosToLongFileName(char *fn)
{
	const int MSIZE = PATH_MAX + 1 - 4; /*  we wanna add ".mp3" later */
	WIN32_FIND_DATAA lpFindFileData;
	HANDLE  h = FindFirstFileA(fn, &lpFindFileData);
	if (h != INVALID_HANDLE_VALUE) {
		int     a;
		char   *q, *p;
		FindClose(h);
		for (a = 0; a < MSIZE; a++) {
			if ('\0' == lpFindFileData.cFileName[a])
				break;
		}
		if (a >= MSIZE || a == 0)
			return;
		q = strrchr(fn, '\\');
		p = strrchr(fn, '/');
		if (p - q > 0)
			q = p;
		if (q == NULL)
			q = strrchr(fn, ':');
		if (q == NULL)
			strncpy(fn, lpFindFileData.cFileName, a);
		else {
			a += q - fn + 1;
			if (a >= MSIZE)
				return;
			strncpy(++q, lpFindFileData.cFileName, MSIZE - a);
		}
	}
}

BOOL
SetPriorityClassMacro(DWORD p)
{
	HANDLE  op = GetCurrentProcess();
	return SetPriorityClass(op, p);
}

void
setProcessPriority(int Priority)
{
	switch (Priority) {
	case 0:
	case 1:
		SetPriorityClassMacro(IDLE_PRIORITY_CLASS);
		console_printf("==> Priority set to Low.\n");
		break;
	default:
	case 2:
		SetPriorityClassMacro(NORMAL_PRIORITY_CLASS);
		console_printf("==> Priority set to Normal.\n");
		break;
	case 3:
	case 4:
		SetPriorityClassMacro(HIGH_PRIORITY_CLASS);
		console_printf("==> Priority set to High.\n");
		break;
	}
}
#endif


#if defined(__OS2__)
/* OS/2 priority functions */
static void
setProcessPriority(int Priority)
{
	int     rc;

	switch (Priority) {

	case 0:
		rc = DosSetPriority(0, /* Scope: only one process */
			PRTYC_IDLE, /* select priority class (idle, regular, etc) */
			0, /* set delta */
			0); /* Assume current process */
		console_printf("==> Priority set to 0 (Low priority).\n");
		break;

	case 1:
		rc = DosSetPriority(0, /* Scope: only one process */
			PRTYC_IDLE, /* select priority class (idle, regular, etc) */
			PRTYD_MAXIMUM, /* set delta */
			0); /* Assume current process */
		console_printf("==> Priority set to 1 (Medium priority).\n");
		break;

	case 2:
		rc = DosSetPriority(0, /* Scope: only one process */
			PRTYC_REGULAR, /* select priority class (idle, regular, etc) */
			PRTYD_MINIMUM, /* set delta */
			0); /* Assume current process */
		console_printf("==> Priority set to 2 (Regular priority).\n");
		break;

	case 3:
		rc = DosSetPriority(0, /* Scope: only one process */
			PRTYC_REGULAR, /* select priority class (idle, regular, etc) */
			0, /* set delta */
			0); /* Assume current process */
		console_printf("==> Priority set to 3 (High priority).\n");
		break;

	case 4:
		rc = DosSetPriority(0, /* Scope: only one process */
			PRTYC_REGULAR, /* select priority class (idle, regular, etc) */
			PRTYD_MAXIMUM, /* set delta */
			0); /* Assume current process */
		console_printf("==> Priority set to 4 (Maximum priority). I hope you enjoy it :)\n");
		break;

	default:
		console_printf("==> Invalid priority specified! Assuming idle priority.\n");
	}
}
#endif


/***********************************************************************
*
*  Message Output
*
***********************************************************************/


#if defined( _WIN32 ) && !defined(__MINGW32__)
/* Idea for unicode support in LAME, work in progress
* - map UTF-16 to UTF-8
* - advantage, the rest can be kept unchanged (mostly)
* - make sure, fprintf on console is in correct code page
*   + normal text in source code is in ASCII anyway
*   + ID3 tags and filenames coming from command line need attention
* - call wfopen with UTF-16 names where needed
*
* why not wchar_t all the way?
* well, that seems to be a big mess and not portable at all
*/
#include <wchar.h>
#include <mbstring.h>

static wchar_t *mbsToUnicode(const char *mbstr, int code_page)
{
	int n = MultiByteToWideChar(code_page, 0, mbstr, -1, NULL, 0);
	wchar_t* wstr = malloc(n * sizeof(wstr[0]));
	if (wstr != 0) {
		n = MultiByteToWideChar(code_page, 0, mbstr, -1, wstr, n);
		if (n == 0) {
			free(wstr);
			wstr = 0;
		}
	}
	return wstr;
}

static char *unicodeToMbs(const wchar_t *wstr, int code_page)
{
	int n = 1 + WideCharToMultiByte(code_page, 0, wstr, -1, 0, 0, 0, 0);
	char* mbstr = malloc(n * sizeof(mbstr[0]));
	if (mbstr != 0) {
		n = WideCharToMultiByte(code_page, 0, wstr, -1, mbstr, n, 0, 0);
		if (n == 0) {
			free(mbstr);
			mbstr = 0;
		}
	}
	return mbstr;
}

char* mbsToMbs(const char* str, int cp_from, int cp_to)
{
	wchar_t* wstr = mbsToUnicode(str, cp_from);
	if (wstr != 0) {
		char* local8bit = unicodeToMbs(wstr, cp_to);
		free(wstr);
		return local8bit;
	}
	return 0;
}

enum { cp_utf8, cp_console, cp_actual };

wchar_t *utf8ToUnicode(const char *mbstr)
{
	return mbsToUnicode(mbstr, CP_UTF8);
}

char *unicodeToUtf8(const wchar_t *wstr)
{
	return unicodeToMbs(wstr, CP_UTF8);
}

char* utf8ToLocal8Bit(const char* str)
{
	return mbsToMbs(str, CP_UTF8, CP_ACP);
}

char* utf8ToConsole8Bit(const char* str)
{
	return mbsToMbs(str, CP_UTF8, GetConsoleOutputCP());
}

char* local8BitToUtf8(const char* str)
{
	return mbsToMbs(str, CP_ACP, CP_UTF8);
}

char* console8BitToUtf8(const char* str)
{
	return mbsToMbs(str, GetConsoleOutputCP(), CP_UTF8);
}

char* utf8ToLatin1(char const* str)
{
	return mbsToMbs(str, CP_UTF8, 28591); /* Latin-1 is code page 28591 */
}

unsigned short* utf8ToUtf16(char const* mbstr) /* additional Byte-Order-Marker */
{
	int n = MultiByteToWideChar(CP_UTF8, 0, mbstr, -1, NULL, 0);
	wchar_t* wstr = malloc((n + 1) * sizeof(wstr[0]));
	if (wstr != 0) {
		wstr[0] = 0xfeff; /* BOM */
		n = MultiByteToWideChar(CP_UTF8, 0, mbstr, -1, wstr + 1, n);
		if (n == 0) {
			free(wstr);
			wstr = 0;
		}
	}
	return wstr;
}





int wmain(int argc, wchar_t* argv[])
{
	char **utf8_argv;
	int i, ret;

	utf8_argv = calloc(argc, sizeof(char*));
	for (i = 0; i < argc; ++i) {
		utf8_argv[i] = unicodeToUtf8(argv[i]);
	}
	ret = c_main(argc, utf8_argv);
	for (i = 0; i < argc; ++i) {
		free(utf8_argv[i]);
	}
	free(utf8_argv);
	return ret;
}

FILE* lame_fopen(char const* file, char const* mode)
{

	FILE* fh = 0;
	wchar_t* wfile = utf8ToUnicode(file);
	wchar_t* wmode = utf8ToUnicode(mode);
	if (wfile != 0 && wmode != 0) {
		fh = _wfopen(wfile, wmode);
	}
	else {
		fh = fopen(file, mode);
	}
	free(wfile);
	free(wmode);
	return fh;
}

char* lame_getenv(char const* var)
{
	char* str = 0;
	wchar_t* wvar = utf8ToUnicode(var);
	wchar_t* wstr = 0;
	if (wvar != 0) {
		wstr = _wgetenv(wvar);
		str = unicodeToUtf8(wstr);
	}
	free(wvar);
	free(wstr);
	return str;
}

#else

FILE* lame_fopen(char const* file, char const* mode)
{
	printf("File lame_fopen: %s \n", file);

	return fopen(file, mode);
}

char* lame_getenv(char const* var)
{
	char* str = getenv(var);
	if (str) {
		return strdup(str);
	}
	return 0;
}
/*
int main(int argc, char *argv[])
{
return c_main(argc, argv);
}*/

#endif





double cmax(double a, double b) {
	return (a > b) ? a : b;
}

double cmin(double a, double b) {
	return (a < b) ? a : b;
}

double
max_e(double a, double b, double c, int *index_max) {
	double x = cmax(a, b);

	if (x == a) *index_max = 0;
	else *index_max = 1;

	double y = cmax(x, c);

	if (y == c) *index_max = 2;

	return y;
}

double
min_e(double a, double b, double c, int *index_min) {
	double x = cmin(a, b);

	if (x == a) *index_min = 0;
	else *index_min = 1;

	double y = cmin(x, c);

	if (y == c) *index_min = 2;

	return y;
}

void
amplitude_hiding(int to_hide[], int l_sound[], int r_sound[], int lr_sound_size, int elem_to_hide);
void
calculate_e_values(double *e1, double *e2, double *e3, double *e_max, double *e_min, double *e_mid,
	int l_sound[], int r_sound[], int section1_size, int section2_size, int section3_size, int *index_max, int *index_mid, int *index_min, int number_of_gos);
void
modify_amplitude(int l_sound[], int length, double omega, int channel, int offset);

int*
extraction_data(lame_t gf, int l_sound[], int r_sound[], int lr_sound_size);

char* bit_to_string(int *bits) {

	int i = 0, j = 0;
	int length = sizeof(bits) * sizeof(bits[0]);


	char *string;

	int temp = 0;

	string = malloc(sizeof(char) * length);

	for (; i < length / 8; i++) {

		for (j = 0; j < 8; j++) {
			temp = (temp << 1) | bits[i * 8 + j];
		}

		string[i] = (char)temp;
		temp = 0;
	}
	return string;
}



/*
gf -> for getaudio
l_sound r_sound -> left and right channel
lr_soundsize -> size of song
*/
int*
extraction_data(lame_t gf, int l_sound[], int r_sound[], int lr_sound_size)
{
	int i = 0;

	int number_of_gos = lr_sound_size / SAMPLES_PER_GOS;

	int* array_of_byte = malloc(number_of_gos * sizeof(int));
	//The size of section of gos could be different from each otnd[]her. We use, at this moment, the same size for each one.

	int section1_size = SAMPLES_PER_GOS_SECTION,
		section2_size = SAMPLES_PER_GOS_SECTION,
		section3_size = SAMPLES_PER_GOS_SECTION;


	// Finish when all gos are analized.
	for (; i < number_of_gos; i++) {


		//every parameter is an array to consider both channels
		double e1[2] = { 0, 0 }, e2[2] = { 0, 0 }, e3[2] = { 0, 0 },
			e_max[2] = { 0, 0 }, e_min[2] = { 0, 0 }, e_mid[2] = { 0, 0 };//, E[2] = { 0, 0 } , *e[3] = { e1, e2, e3 };
		int index_max[2] = { 0,0 }, index_min[2] = { 0,0 }, index_mid[2] = { 0,0 }, l[3] = { section1_size, section2_size, section3_size };

		calculate_e_values(e1, e2, e3, e_max, e_min, e_mid, l_sound, r_sound, section1_size, section2_size, section3_size, index_max, index_mid, index_min, i);

		double A[2] = { e_max[0] - e_mid[0], e_max[1] - e_mid[1] },
			B[2] = { e_mid[0] - e_min[0], e_mid[1] - e_min[1] };

		// Add channel 2
		if (A[0] >= B[0]) {

			array_of_byte[i] = 1;
			//printf("1\n");
		}
		else if (B[0] > A[0]) {
			array_of_byte[i] = 0;
			//printf("0\n");
		}
	}
	return array_of_byte;

}

/*
to_hide - array of int containing the bits of data to hide. Maybe we can compress it to increase amount of hidden data
lr_sound - array containing the wav samples, [0][] left channel, [1][] right channel
lr_sound_size - #samples in lr_sound per channel
*/
void
amplitude_hiding(int to_hide[], int l_sound[], int r_sound[], int lr_sound_size, int elem_to_hide) {

	int i = 0;

	int number_of_gos = lr_sound_size / SAMPLES_PER_GOS, gos_start = 0;

	//The size of section of gos could be different from each other. We use, at this moment, the same size for each one.
	int section1_size = SAMPLES_PER_GOS_SECTION,
		section2_size = SAMPLES_PER_GOS_SECTION,
		section3_size = SAMPLES_PER_GOS_SECTION;

	// Variabile usata per avanzamento GOS
	int L = section1_size + section2_size + section3_size;

	for (; i < number_of_gos && i < elem_to_hide; i++) {

		//every parameter is an array to consider both channels
		double e1[2] = { 0, 0 }, e2[2] = { 0, 0 }, e3[2] = { 0, 0 },
			e_max[2] = { 0, 0 }, e_min[2] = { 0, 0 }, e_mid[2] = { 0, 0 };//, E[2] = { 0, 0 } , *e[3] = { e1, e2, e3 };
		int index_max[2] = { 0,0 }, index_min[2] = { 0,0 }, index_mid[2] = { 0,0 }, l[3] = { section1_size, section2_size, section3_size };

		double d = 0.01;

		// Flag per il while
		int flag = 1;
		calculate_e_values(e1, e2, e3, e_max, e_min, e_mid, l_sound, r_sound, section1_size, section2_size, section3_size, index_max, index_mid, index_min, i);

		double THRESHOLD[2] = { (e_max[LEFT] + (2 * e_mid[LEFT]) + e_min[LEFT]) * d, (e_max[R] + (2 * e_mid[R]) + e_min[R]) * d };

		//printf("Il valore del THRESHOLD prima del while è : %1.10f - %1.10f \n \n ", THRESHOLD[LEFT], THRESHOLD[R]);
		while (flag) {
			// Inizializza le variabili ad ogni ciclo
			double delta[2] = { 0,0 }, omega_up[2] = { 0,0 }, omega_down[2] = { 0,0 };
			int offset_up[2] = { gos_start,gos_start }, offset_down[2] = { gos_start, gos_start };

			calculate_e_values(e1, e2, e3, e_max, e_min, e_mid, l_sound, r_sound, section1_size, section2_size, section3_size, index_max, index_mid, index_min, i);

			//printf("\n VALUES: \n\n\n%Lf - %Lf - %Lf - %Lf - %Lf - %Lf - %d - %d - %d - %d - %d - %d \n\n\n\n\n\n",
			//      e1[0], e2[0], e3[0], e_max[0], e_min[0], e_mid[0], section1_size, section2_size, section3_size, index_max[0], index_mid[0], index_min[0]);

			// Calcola A e B in base ai valori calcolati da calculate_e_values()
			double A[2] = { e_max[0] - e_mid[0], e_max[1] - e_mid[1] },
				B[2] = { e_mid[0] - e_min[0], e_mid[1] - e_min[1] };

			//double D[2] = { abs(A[0] - B[0]), abs(A[1] - B[1]) };

			// Calcola THRESHOLD in base ai valori di calculate_e_values()

			double subtraction[2] = { 0, 0 };
			int success[2] = { 0, 0 };
//			printf("IL bit da nascondere è : %d \n", to_hide[i]);

			if (to_hide[i]) {
				subtraction[LEFT] = A[LEFT] - B[LEFT];
				subtraction[R] = A[R] - B[R];
			}
			else {
				subtraction[LEFT] = B[LEFT] - A[LEFT];
				subtraction[R] = B[R] - A[R];
			}
			success[LEFT] = (subtraction[LEFT] >= THRESHOLD[LEFT]);
			success[R] = (subtraction[R] >= THRESHOLD[R]);

			//printf("Il valore della sottrazione è : %1.10f\n", subtraction);

			/* DEBUG ZONE */
			//printf("Valore subtraction : %1.50f Valore THRESHOLD : %1.50f \n", subtraction, THRESHOLD);

			// Controlla superamento soglia
			if (success[LEFT] && success[R]) { // se soglia superata
				flag = 0;
				break; // esci immediatamente
			}

			//Inizio modifica canale L
			if (!success[LEFT]) {
				
				delta[LEFT] = (THRESHOLD[LEFT] - subtraction[LEFT]) / 3;

				//				printf("IL valore del delta %d\n", delta);
				//CALCOLO DELTA
				if (to_hide[i]) {

					//modifico parametri e
					e_max[0] += delta[LEFT];
					e_mid[0] -= delta[LEFT];

					//	printf("Il valore di OMEGA UP è: %1.10f\n", omega_up);
					//	printf("Il valore di OMEGA DOWN è: %1.10f\n\n \n \n", omega_down);


					//calcolo variazione ampiezza
					omega_up[LEFT] = 1 + (delta[LEFT] / e_max[0]);
					omega_down[LEFT] = 1 - (delta[LEFT] / e_mid[0]);

					int i = 0;
					for (; i < index_max[0]; i++) {
						offset_up[LEFT] += l[i];
					}
					for (i = 0; i < index_mid[0]; i++) {
						offset_down[LEFT] += l[i];
					}
					modify_amplitude(l_sound, l[index_max[0]], omega_up[LEFT], LEFT, offset_up[LEFT]);
					//		printf("l_sound[%d]:%Lf - ", offset_up, l_sound[offset_up]);
					modify_amplitude(l_sound, l[index_mid[0]], omega_down[LEFT], LEFT, offset_down[LEFT]);
					//			printf("l_sound[%d]:%Lf \n ", offset_down, l_sound[offset_down]);
				}
				else { //SE DEVO NASCONDERE 0
					e_mid[0] += delta[LEFT];
					e_min[0] -= delta[LEFT];
					omega_up[LEFT] = 1 + (delta[LEFT] / e_mid[0]);
					omega_down[LEFT] = 1 - (delta[LEFT] / e_min[0]);

					//printf("Il valore diDELTA è: %1.10f\n", delta[LEFT]);

					//printf("Il valore di OMEGA UP è: %1.10f\n", omega_up[LEFT]);
					//printf("Il valore di OMEGA DOWN è: %1.10f\n \n \n \n", omega_down[LEFT]);

					int i = 0;
					for (; i < index_mid[0]; i++) {
						offset_up[LEFT] += l[i];
					}
					for (i = 0; i < index_min[0]; i++) {
						offset_down[LEFT] += l[i];
					}
					modify_amplitude(l_sound, l[index_mid[0]], omega_up[LEFT], LEFT, offset_up[LEFT]);
					modify_amplitude(l_sound, l[index_min[0]], omega_down[LEFT], LEFT, offset_down[LEFT]);

				}
				//sleep(2);
			} // end if success

			//Inizio modifica canale R
			if (!success[R]) { 

				delta[R] = (THRESHOLD[R] - subtraction[R]) / 3;

				//				printf("IL valore del delta %d\n", delta);
				//CALCOLO DELTA
				if (to_hide[i]) {

					//modifico parametri e
					e_max[R] += delta[R];
					e_mid[R] -= delta[R];

					//	printf("Il valore di OMEGA UP è: %1.10f\n", omega_up);
					//	printf("Il valore di OMEGA DOWN è: %1.10f\n\n \n \n", omega_down);


					//calcolo variazione ampiezza
					omega_up[R] = 1 + (delta[R] / e_max[R]);
					omega_down[R] = 1 - (delta[R] / e_mid[R]);

					int i = 0;
					for (; i < index_max[R]; i++) {
						offset_up[R] += l[i];
					}
					for (i = 0; i < index_mid[R]; i++) {
						offset_down[R] += l[i];
					}
					modify_amplitude(r_sound, l[index_max[R]], omega_up[R], LEFT, offset_up[R]);
					//		printf("l_sound[%d]:%Lf - ", offset_up, l_sound[offset_up]);
					modify_amplitude(r_sound, l[index_mid[R]], omega_down[R], LEFT, offset_down[R]);
					//			printf("l_sound[%d]:%Lf \n ", offset_down, l_sound[offset_down]);
				}
				else { //SE DEVO NASCONDERE 0
					e_mid[R] += delta[R];
					e_min[R] -= delta[R];
					omega_up[R] = 1 + (delta[R] / e_mid[R]);
					omega_down[R] = 1 - (delta[R] / e_min[R]);

					//printf("Il valore diDELTA è: %1.10f\n", delta[R]);

					//printf("Il valore di OMEGA UP è: %1.10f\n", omega_up[R]);
					//printf("Il valore di OMEGA DOWN è: %1.10f\n \n \n \n", omega_down[R]);

					int i = 0;
					for (; i < index_mid[R]; i++) {
						offset_up[R] += l[i];
					}
					for (i = 0; i < index_min[R]; i++) {
						offset_down[R] += l[i];
					}
					modify_amplitude(r_sound, l[index_mid[R]], omega_up[R], LEFT, offset_up[R]);
					modify_amplitude(r_sound, l[index_min[R]], omega_down[R], LEFT, offset_down[R]);

				}
				//sleep(2);
			}
		} // end while
		  
		  // A fine while, cambia il gos
		gos_start += L - 1;
	} // end for
} // end function

void
calculate_e_values(double *e1, double *e2, double *e3, double *e_max, double *e_min, double *e_mid,
	int l_sound[], int r_sound[], int section1_size, int section2_size, int section3_size, int *index_max, int *index_mid, int *index_min, int number_of_gos) {

	int L = section1_size + section2_size + section3_size;


	int j = 0;
	int i = number_of_gos;

	for (; j < section1_size; j++) {

		e1[0] += abs(l_sound[L * i + j]);
		e1[1] += abs(r_sound[L * i + j]);

	}

	int stop_condition = section1_size + section2_size - 1;

	for (j = section1_size; j < stop_condition; j++) {

		e2[0] += abs(l_sound[L * i + j]);
		e2[1] += abs(r_sound[L * i + j]);
	}

	stop_condition = L;

	for (j = section1_size + section2_size; j < stop_condition; j++) {

		e3[0] += abs(l_sound[L * i + j]);
		e3[1] += abs(r_sound[L * i + j]);
	}

	e1[0] = e1[0] / section1_size;
	e1[1] = e1[1] / section1_size;


	e2[0] = e2[0] / section2_size;
	e2[1] = e2[1] / section2_size;


	e3[0] = e3[0] / section3_size;
	e3[1] = e3[1] / section3_size;

	e_max[0] = max_e(e1[0], e2[0], e3[0], &index_max[0]);

	//printf("max[0] = %f\n", e_max[0]);

	e_min[0] = min_e(e1[0], e2[0], e3[0], &index_min[0]);

	if (e_max[0] != e1[0] && e_min[0] != e1[0]) {
		e_mid[0] = e1[0];
		index_mid[0] = 0;
	}
	else
		if (e_max[0] != e2[0] && e_min[0] != e2[0]) {
			e_mid[0] = e2[0];
			index_mid[0] = 1;
		}

		else
			if (e_max[0] != e3[0] && e_min[0] != e3[0]) {
				e_mid[0] = e3[0];
				index_mid[0] = 2;
			}


	e_max[1] = max_e(e1[1], e2[1], e3[1], &index_max[1]);
	e_min[1] = min_e(e1[1], e2[1], e3[1], &index_min[1]);

	if (e_max[1] != e1[1] && e_min[1] != e1[1]) {
		e_mid[1] = e1[1];
		index_mid[1] = 0;
	}
	else
		if (e_max[1] != e2[1] && e_min[1] != e2[1]) {
			e_mid[1] = e2[1];
			index_mid[1] = 1;
		}

		else
			if (e_max[1] != e3[1] && e_min[1] != e3[1]) {
				e_mid[1] = e3[1];
				index_mid[1] = 2;
			}
}

void
modify_amplitude(int l_sound[], int length, double omega, int channel, int offset)
{
	int i = 0;

	for (; i < length; i++) {
		l_sound[offset + i] = l_sound[offset + i] * omega;
	}
}

FILE* prepareEncoding(lame_t gf, char *inPath, char *outPath) {

	int     ret;
	FILE   *outf = NULL;

	lame_set_msgf(gf, &frontend_msgf);
	lame_set_errorf(gf, &frontend_errorf);
	lame_set_debugf(gf, &frontend_debugf);


	printf("outPath: %s\n", outPath);
	printf("inPath: %s\n", inPath);

	//memset(inPath, 0, sizeof(inPath));

	if (init_infile(gf, inPath) < 0) {
		error_printf("Can't init infile '%s'\n", inPath);
		return NULL;
	}

	printf("File aperto\n");

	if (outPath != NULL)
		if ((outf = init_outfile(outPath, lame_get_decode_only(gf))) == NULL) {
			error_printf("Can't init outfile '%s'\n", outPath);
			return NULL;
		}

	return outf;
}

void
print_trailing_info(lame_global_flags * gf)
{
	if (lame_get_findReplayGain(gf)) {
		int     RadioGain = lame_get_RadioGain(gf);
		console_printf("ReplayGain: %s%.1fdB\n", RadioGain > 0 ? "+" : "",
			((float)RadioGain) / 10.0);
		if (RadioGain > 0x1FE || RadioGain < -0x1FE)
			error_printf
			("WARNING: ReplayGain exceeds the -51dB to +51dB range. Such a result is too\n"
				"         high to be stored in the header.\n");
	}

	/* if (the user requested printing info about clipping) and (decoding
	on the fly has actually been performed) */
	if (global_ui_config.print_clipping_info && lame_get_decode_on_the_fly(gf)) {
		float   noclipGainChange = (float)lame_get_noclipGainChange(gf) / 10.0f;
		float   noclipScale = lame_get_noclipScale(gf);

		if (noclipGainChange > 0.0) { /* clipping occurs */
			console_printf
			("WARNING: clipping occurs at the current gain. Set your decoder to decrease\n"
				"         the  gain  by  at least %.1fdB or encode again ", noclipGainChange);

			/* advice the user on the scale factor */
			if (noclipScale > 0) {
				console_printf("using  --scale %.2f\n", noclipScale);
				console_printf("         or less (the value under --scale is approximate).\n");
			}
			else {
				/* the user specified his own scale factor. We could suggest
				* the scale factor of (32767.0/gfp->PeakSample)*(gfp->scale)
				* but it's usually very inaccurate. So we'd rather advice him to
				* disable scaling first and see our suggestion on the scale factor then. */
				console_printf("using --scale <arg>\n"
					"         (For   a   suggestion  on  the  optimal  value  of  <arg>  encode\n"
					"         with  --scale 1  first)\n");
			}

		}
		else {          /* no clipping */
			if (noclipGainChange > -0.1)
				console_printf
				("\nThe waveform does not clip and is less than 0.1dB away from full scale.\n");
			else
				console_printf
				("\nThe waveform does not clip and is at least %.1fdB away from full scale.\n",
					-noclipGainChange);
		}
	}

}


int
write_xing_frame(lame_global_flags * gf, FILE * outf, size_t offset)
{
	unsigned char mp3buffer[LAME_MAXMP3BUFFER];
	size_t  imp3, owrite;

	imp3 = lame_get_lametag_frame(gf, mp3buffer, sizeof(mp3buffer));
	if (imp3 <= 0) {
		return 0;       /* nothing to do */
	}
	if (global_ui_config.silent <= 0) {
		console_printf("Writing LAME Tag...");
	}
	if (imp3 > sizeof(mp3buffer)) {
		error_printf
		("Error writing LAME-tag frame: buffer too small: buffer size=%d  frame size=%d\n",
			sizeof(mp3buffer), imp3);
		return -1;
	}
	if (fseek(outf, offset, SEEK_SET) != 0) {
		error_printf("fatal error: can't update LAME-tag frame!\n");
		return -1;
	}
	owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
	if (owrite != imp3) {
		error_printf("Error writing LAME-tag \n");
		return -1;
	}
	if (global_ui_config.silent <= 0) {
		console_printf("done\n");
	}
	return imp3;
}

int
write_id3v1_tag(lame_t gf, FILE * outf)
{
	unsigned char mp3buffer[128];
	int     imp3, owrite;

	imp3 = lame_get_id3v1_tag(gf, mp3buffer, sizeof(mp3buffer));
	if (imp3 <= 0) {
		return 0;
	}
	if ((size_t)imp3 > sizeof(mp3buffer)) {
		error_printf("Error writing ID3v1 tag: buffer too small: buffer size=%d  ID3v1 size=%d\n",
			sizeof(mp3buffer), imp3);
		return 0;       /* not critical */
	}
	owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
	if (owrite != imp3) {
		error_printf("Error writing ID3v1 tag \n");
		return 1;
	}
	return 0;
}

int encode_hiding(int *to_hide, int tohide_lenght, lame_t gf, FILE * outf, char *inPath, char *outPath)
{



	unsigned char mp3buffer[LAME_MAXMP3BUFFER];
	int     Buffer[2][1152];
	int     iread, imp3, owrite;
	size_t  id3v2_size;

	encoder_progress_begin(gf, inPath, outPath);

	id3v2_size = lame_get_id3v2_tag(gf, 0, 0);
	if (id3v2_size > 0) {
		unsigned char *id3v2tag = malloc(id3v2_size);
		if (id3v2tag != 0) {
			imp3 = lame_get_id3v2_tag(gf, id3v2tag, id3v2_size);
			owrite = (int)fwrite(id3v2tag, 1, imp3, outf);
			free(id3v2tag);
			if (owrite != imp3) {
				encoder_progress_end(gf);
				error_printf("Error writing ID3v2 tag \n");
				return 1;
			}
		}
	}
	else {
		unsigned char* id3v2tag = getOldTag(gf);
		id3v2_size = sizeOfOldTag(gf);
		if (id3v2_size > 0) {
			size_t owrite = fwrite(id3v2tag, 1, id3v2_size, outf);
			if (owrite != id3v2_size) {
				encoder_progress_end(gf);
				error_printf("Error writing ID3v2 tag \n");
				return 1;
			}
		}
	}
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}

	int offset = 0;


	int *bit_to_hide, num_bit_to_encode_iter = 0;

	do {

		/* read in 'iread' samples */
		iread = get_audio(gf, Buffer);

		if (iread >= 0) {

			if (offset < tohide_lenght) {

				int max_bit_to_hide = iread / SAMPLES_PER_GOS;

				if ((tohide_lenght - offset) / max_bit_to_hide < 1)
					num_bit_to_encode_iter = (tohide_lenght - offset) % max_bit_to_hide;
				else
					num_bit_to_encode_iter = max_bit_to_hide;

				bit_to_hide = malloc(sizeof(int)* num_bit_to_encode_iter);

				int xx = 0, yy = 0;

				for (xx = offset; xx < offset + num_bit_to_encode_iter; xx++) {
					bit_to_hide[yy++] = to_hide[xx];
				}

				offset += num_bit_to_encode_iter;


				//printf("Offset: %d | num_bit_to_encode_iter: %d\n", offset, num_bit_to_encode_iter);
				  
				amplitude_hiding(bit_to_hide, Buffer[0], Buffer[1], iread, num_bit_to_encode_iter);

				int *xxx = extraction_data(gf, Buffer[0], Buffer[1], iread);

				int ggg = 0;
				for (; ggg < num_bit_to_encode_iter; ggg++) {
					if (bit_to_hide[ggg] != xxx[ggg])
						printf("\nErrore nella codifica del bit: %d %d-%d", ggg, bit_to_hide[ggg], xxx[ggg]);
					//else printf("\nBit codificato correttamente: %d %d-%d", ggg, bit_to_hide[ggg], xxx[ggg]);
					
				}
			}

			encoder_progress(gf);

			//printf("Offset: %d | num_bit_to_encode_iter: %d - %d \n", offset, num_bit_to_encode_iter, tohide_lenght);


			imp3 = lame_encode_buffer_int(gf, Buffer[0], Buffer[1], iread,
				mp3buffer, sizeof(mp3buffer));

			/* was our output buffer big enough? */
			if (imp3 < 0) {
				if (imp3 == -1)
					error_printf("mp3 buffer is not big enough... \n");
				else
					error_printf("mp3 internal error:  error code=%i\n", imp3);
				return 1;
			}
			owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
			if (owrite != imp3) {
				error_printf("Error writing mp3 output \n");
				return 1;
			}
		}
		if (global_writer.flush_write == 1) {
			fflush(outf);
		}
	} while (iread > 0);

	imp3 = lame_encode_flush(gf, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */


	if (imp3 < 0) {
		if (imp3 == -1)
			error_printf("mp3 buffer is not big enough... \n");
		else
			error_printf("mp3 internal error:  error code=%i\n", imp3);
		return 1;

	}

	encoder_progress_end(gf);

	owrite = (int)fwrite(mp3buffer, 1, imp3, outf);
	if (owrite != imp3) {
		error_printf("Error writing mp3 output \n");
		return 1;
	}
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}
	imp3 = write_id3v1_tag(gf, outf);
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}
	if (imp3) {
		return 1;
	}
	write_xing_frame(gf, outf, id3v2_size);
	if (global_writer.flush_write == 1) {
		fflush(outf);
	}
	if (global_ui_config.silent <= 0) {
		print_trailing_info(gf);
	}
	fclose(outf); 
	return 0;
}

void decode_hiding(lame_t gf, char *inPath) {
	//prepareEncoding(gf, inPath, NULL);

	int     Buffer[2][1105], iread;
	do {

		/* read in 'iread' samples */
		iread = get_audio16(gf, Buffer);

		if (iread >= 0) {

			int *hidden_data = extraction_data(gf, Buffer[0], Buffer[1], iread);
			printf("%s\n", bit_to_string(hidden_data));


		}

	} while (iread > 0);

}


int
main(int argc, char *argv[]) {

	lame_t  gf;
	int     ret;


#if macintosh
	argc = ccommand(&argv);
#endif
#ifdef __EMX__
	/* This gives wildcard expansion on Non-POSIX shells with OS/2 */
	_wildcard(&argc, &argv);
#endif
#if defined( _WIN32 ) && !defined(__MINGW32__)
	set_process_affinity();
#endif


	int sflag = 0;
	int kflag = 0;
	int fflag = 0;
	int eflag = 0;
	int dflag = 0;
	char *svalue = NULL, *kvalue = NULL, *fvalue = NULL, *evalue = NULL;
	int c;


	/*
	-s - string to hide
	-k - pem file
	-f - .wav input file

	*/

	int *tohide_int;
	int tohide_lenght;

	while ((c = getopt(argc, argv, "s:k:f:ed")) != -1)
		switch (c)
		{
		case 's':
			sflag = 1;
			svalue = optarg;
			// Numero di elementi da inserire nell'array
			tohide_lenght = strlen(svalue) * 8;

			// Array di interi per nascondere i bit
			tohide_int = malloc(sizeof(int) * tohide_lenght);

			// Inner poner
			int i, j = 0;
			for (; *svalue != 0; ++svalue) {
				// AND Bitwise di ogni carattere
				for (i = 7; i >= 0; --i) {
					if (*svalue & 1 << i) {
						tohide_int[j] = 1;
						//*svalue = putchar('1'); // DEBUG
					}
					else {
						tohide_int[j] = 0;
						//*svalue = putchar('0'); // DEBUGs
					}

					// Incremento del puntatore
					j++;
				}
			}
			break;
		case 'k':
			kflag = 1;
			kvalue = optarg;
			break;
		case 'f':
			fflag = 1;
			fvalue = optarg;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case '?':
			if (optopt == 's' || optopt == 'k' || optopt == 'f')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option -%c'.\n", optopt);
			else
				fprintf(stderr,
					"Unknown option character `\\x%x'.\n",
					optopt);
			return 1;
		default:
			abort();
		}

	frontend_open_console();

	gf = lame_init(); /* initialize libmp3lamampe */

	if (NULL == gf) {
		error_printf("fatal error during initialization\n");
		return -1;
	}
	else {

		lame_init_params(gf);

		//lame_set_mode(gf, 0);
		lame_set_brate(gf, 128);


		if (eflag && dflag) {
			printf("Only one of e and d may be used\n");
			return -1;
		}

		if (!(eflag || dflag)) {
			printf("One of e and d must be used\n");
			return -1;
		}

		if (!sflag && !fflag && !kflag) {
			printf("Usage lame -e/-d -f file -s string_to_hide -k .pem_file\n");
			return -1;
		}

		//se devo codificare
		if (eflag) {

			char    inPath[PATH_MAX + 1];
			char    outPath[PATH_MAX + 1];

			char temp[PATH_MAX + 1];
			strcpy(temp, fvalue);
			strcat(temp, ".wav");
			strcpy(inPath, temp);


			char temp2[PATH_MAX + 1];
			strcpy(temp2, strcat(fvalue, ".mp3"));
			strcpy(outPath, temp2);

			FILE *outf = prepareEncoding(gf, inPath, outPath);


			encode_hiding(tohide_int, tohide_lenght, gf, outf, inPath, outPath);
		}
		//altrimenti devo decodificare
		else
		{
			char    inPath[PATH_MAX + 1];
			char temp2[PATH_MAX + 1];
			strcpy(temp2, strcat(fvalue, ".mp3"));
			strcpy(inPath, temp2);

			prepareEncoding(gf, inPath, NULL);
			printf("inizio decodifica\n");
			decode_hiding(gf, inPath);
		}

		lame_close(gf);

	}

	frontend_close_console();

	return ret;
}