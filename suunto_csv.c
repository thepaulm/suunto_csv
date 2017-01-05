#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include "libdivecomputer.h"

const char SUUNTO[] = "Suunto";
const char BASE_DEV[] = "tty.usbserial";

dc_descriptor_t *
get_descriptor(const char *model)
{
	dc_status_t rc;
	dc_iterator_t *i = NULL;
	dc_descriptor_t *d;

	rc = dc_descriptor_iterator(&i);
	if (rc != DC_STATUS_SUCCESS) {
		return NULL;
	}
	rc = dc_iterator_next(i, &d);
	while (rc == DC_STATUS_SUCCESS) {
		if (!strcmp(dc_descriptor_get_vendor(d), SUUNTO) &&
			!strcmp(dc_descriptor_get_product(d), model)) {
			break;
		}
		dc_descriptor_free(d);
		d = NULL;
		rc = dc_iterator_next(i, &d);
	}

	dc_iterator_free(i);

	return d;
}

char *
get_tty_dev_name()
{
	DIR *dirp = NULL;
	struct dirent *dp;
	char *name = NULL;

	dirp = opendir("/dev/");
	if (dirp == NULL) {
		fprintf(stderr, "Can't open /dev for reading.\n");
		return NULL;
	}
	while (NULL != (dp = readdir(dirp))) {
		if (!strncmp(dp->d_name, BASE_DEV, strlen(BASE_DEV))) {
			name = (char *)malloc(strlen(dp->d_name) + 1);
			strcpy(name, dp->d_name);
			break;
		}
	}	
	closedir(dirp);
	return name;
}

void
syntax()
{
	printf("suunto_csv: dump dives into csv files\n");
	printf("\tOne file per dive labelled for the date:time\n");
	printf("\n");
	printf("\tUsage: suunto_csv [model]\n");
	printf("\n\tSupported Models: ");

	/* Iterator the descriptors and print out all Suunto models */
	int first = 1;
	dc_status_t rc;
	dc_iterator_t *i = NULL;
	dc_descriptor_t *d = NULL;
	rc = dc_descriptor_iterator(&i);
	if (rc != DC_STATUS_SUCCESS)
		return;
	rc = dc_iterator_next(i, &d);
	while (rc == DC_STATUS_SUCCESS) {
		if (!strcmp(dc_descriptor_get_vendor(d), SUUNTO)) {
			if (!first) {
				printf(", ");
			}
			first = 0;
			printf("%s", dc_descriptor_get_product(d));
		}
		dc_descriptor_free(d);
		rc = dc_iterator_next(i, &d);
	}
	printf("\n");
	dc_iterator_free(i);

}

int
main(int argc, char *argv[])
{
	/* parse command line arguments */
	const char *model = NULL;
	if (argc == 1) {
		syntax();
		return 0;
	}
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h") ||
		    !strcmp(argv[i], "--help")) {
			syntax();
			return 0;
		} else {
			model = argv[i];
		}
	}

	/* open the descriptor for the dive computer */
	dc_descriptor_t *d = get_descriptor(model);
	if (d == NULL) {
		fprintf(stderr, "Unsupported Suunto model %s\n", model);
		return -1;
	}

	/* set up context */
	dc_context_t *context = NULL;
	dc_context_new(&context);

	/* get the tty device name */
	char *ttydev = get_tty_dev_name();
	if (ttydev == NULL) {
		fprintf(stderr, "I can't find the proper tty in /dev.\n");
		return -1;
	}

	free(ttydev);
	
}
