#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include "screen.h"

/*
 * Error handling - originally used PPC assembly to walk the stack frame
 * and find MacsBug routine names, then showed a StandardAlert dialog.
 * For the SDL port, just print the error to stderr and exit.
 */

void HandleError(int id)
{
	fprintf(stderr, "Fatal error: %d\n", id);
	exit(1);
}
