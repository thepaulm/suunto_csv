#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>

#include "libdivecomputer.h"

const char SUUNTO[] = "Suunto";
const char BASE_DEV[] = "tty.usbserial";

#define RC_BAIL_LABEL(_x, _msg, _label) 														\
	if (_x != DC_STATUS_SUCCESS) {											 					\
		fprintf(stderr, _msg);																	\
		goto _label;																			\
	}

typedef struct event_ctx
{
	dc_device_t *dev;
	FILE *fp;
} event_ctx;

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
			name = (char *)malloc(strlen("/dev/") + strlen(dp->d_name) + 1);
			sprintf(name, "/dev/%s", dp->d_name);
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

static void
sample_cb(dc_sample_type_t type, dc_sample_value_t value, void *userdata)
{
	event_ctx *ctx = (event_ctx*)userdata;
	switch (type) {
		case DC_SAMPLE_TIME:
			fprintf(ctx->fp, "%.2d, ", value.time);
			break;
		case DC_SAMPLE_DEPTH:
			fprintf(ctx->fp, "%.2f\n", value.depth);
			break;
		default:
			break;
	}
}

static dc_status_t
output_csv(dc_parser_t *parser, const unsigned char data[], unsigned int size, event_ctx *ctx)
{
	dc_status_t rc;
	dc_datetime_t dt = {0};
	rc = dc_parser_get_datetime(parser, &dt);
	RC_BAIL_LABEL(rc, "Failed to get dive date.\n", fail);

	printf("Got dive: %d-%d-%d %d:%d:%d\n", dt.month, dt.day, dt.year, dt.hour, dt.minute, dt.second);
	char fname[256];
	sprintf(fname, "dive_%d-%d-%d::%d:%d:%d", dt.month, dt.day, dt.year, dt.hour, dt.minute, dt.second);
	ctx->fp = fopen(fname, "w");
	if (ctx->fp == NULL) {
		fprintf(stderr, "cannot create dive file: %s\n", strerror(errno));
		goto fail;
	}

	dc_parser_samples_foreach(parser, sample_cb, ctx);

	fclose(ctx->fp);
	ctx->fp = NULL;

	return DC_STATUS_SUCCESS;

fail:
	return DC_STATUS_DATAFORMAT;
}

static int
dive_cb(const unsigned char *data, unsigned int size, const unsigned char *fingerprint, unsigned int fsize, void *userdata)
{
	dc_parser_t *parser;
	event_ctx *ctx = (event_ctx *)userdata;
	dc_status_t rc;

	rc = dc_parser_new(&parser, ctx->dev);
	RC_BAIL_LABEL(rc, "Failed to create parser.\n", fail);

	rc = dc_parser_set_data(parser, data, size);
	output_csv(parser, data, size, ctx);
	RC_BAIL_LABEL(rc, "Failed to set parser data.\n", fail_parser);

	dc_parser_destroy(parser);

	return 1;

fail_parser:
	dc_parser_destroy(parser);

fail:
	return -1;
}

int
rip_dives(dc_descriptor_t *desc, dc_context_t *dc_ctx, const char *devname)
{
	dc_status_t rc;
	dc_device_t *dev;
	event_ctx ctx = {NULL, NULL};

	rc = dc_device_open(&dev, dc_ctx, desc, devname);
	RC_BAIL_LABEL(rc, "Failed to open device.\n", fail);

	ctx.dev = dev;

	// XXX - you could set a cancel handler here with dc_device_set_cancel
	rc = dc_device_foreach(dev, dive_cb, &ctx);
	RC_BAIL_LABEL(rc, "Failed to register foreach callback.\n", fail);

exit:
	dc_device_close(dev);
	return 0;

fail:
	return -1;
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

	int ret = rip_dives(d, context, ttydev);
	free(ttydev);
	return ret;
	
}
