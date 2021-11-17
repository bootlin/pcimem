#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "pcimem_cmdline.h"

#define PCI_RESOURCE_FMT "/sys/bus/pci/devices/0000:%s/resource%d"

static size_t access_size = 0, xfer_size = 0;
static size_t length;
static off64_t target_addr;
static int file_fd;
static int verbose = 0;
static int pci_resource_fd;

static void usage_err(const char *path)
{
	pcimem_cmdline_print_help();
	exit(1);
}

enum {
	PCI_READ = 0,
	PCI_WRITE,
};

static long long string_to_size(char *str, const char *name)
{
	long long size;
	char last_char;
	char *end_ptr;
	double tmp;
	
	tmp = strtod(str, &end_ptr);
	if (end_ptr == str) {
		fprintf(stderr, "\nWrong size specified\n");
		usage_err(name);
	}

	size = tmp;
	last_char = end_ptr[0];
	switch (last_char) {
		case 'G':
		case 'g':
			size *= 1024;
		case 'M':
		case 'm':
			size *= 1024;
		case 'K':
		case 'k':
			size *= 1024;
			break;
		default:
			break;
	}

	return size;
}


void pcimem_do_read()
{
	void *mmap_base, *read_addr;
	uint64_t read_val = 0;
	unsigned int i;

	mmap_base = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, pci_resource_fd, 0);
	if(mmap_base == (void *) -1) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		exit(1);
	}

	read_addr = mmap_base + target_addr;
	for (i = 0; i < xfer_size; i += access_size) {
		
		if (read_addr > (mmap_base + length - access_size)) {
			fprintf(stderr, "Read outside resource space, stopping\n");
			break;
		}

		switch (access_size) { 
		case 1:
			read_val = *((uint8_t *) read_addr);
			break;
		case 2:
			read_val = *((uint16_t *) read_addr);
			break;
		case 4:
			read_val = *((uint32_t *) read_addr);
			break;
		case 8:
			read_val = *((uint64_t *) read_addr);
			break;
		}
		write(file_fd, &read_val, access_size);

		if (verbose)
			fprintf(stderr, "Read data %" PRIx64 " from address %p\n", read_val, (void *) (read_addr - mmap_base));

		read_addr += access_size;
	}

	munmap(mmap_base, length);

	return;
}

void pcimem_do_write()
{
	void *mmap_base, *write_addr;
	uint64_t write_val = 0;
	unsigned int i;
	ssize_t ret;

	mmap_base = mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, pci_resource_fd, 0);
	if(mmap_base == (void *) -1) {
		fprintf(stderr, "mmap failed: %s\n", strerror(errno));
		exit(1);
	}

	write_addr = mmap_base + target_addr;
	for (i = 0; i < xfer_size; i += access_size) {

		if (write_addr > (mmap_base + length - access_size)) {
			fprintf(stderr, "Write outside resource space, stopping\n");
			break;
		}

		ret = read(file_fd, &write_val, access_size);
		if (ret != access_size) {
			fprintf(stderr, "Fail to read data from file: %zd instead of %zd, %s\n", ret, access_size, strerror(errno));
			break;
		}

		if (verbose)
			fprintf(stderr, "Writing data %" PRIx64 " at address %p\n", write_val, (void *) (write_addr - mmap_base));

		switch (access_size) { 
		case 1:
			*((uint8_t *) write_addr) = (uint8_t) write_val;
			break;
		case 2:
			*((uint16_t *) write_addr) = (uint16_t) write_val;
			break;
		case 4:
			*((uint32_t *) write_addr) = (uint32_t) write_val;
			break;
		case 8:
			*((uint64_t *) write_addr) = (uint64_t) write_val;
			break;
		}
		
		write_addr += access_size;
	}

	munmap(mmap_base, length);

	return;
}


int main(int argc, char **argv)
{
	struct pcimem_args_info args_info;
	char pci_resource_file[1024];
	bool use_standard_io = false;
	off64_t target_addr = -1;
	int direction = -1;
	char *file = NULL;
	struct stat st;

	if (pcimem_cmdline(argc, argv, &args_info) != 0)
		return EXIT_FAILURE;


	if(args_info.write_given && args_info.read_given) {
		fprintf(stderr, "%s: Only one occurence of '--read' ('-r') or '--write' ('-w') must be given\n", argv[0]);
		return EXIT_FAILURE;
	}

	if(!args_info.write_given && !args_info.read_given) {
		fprintf(stderr, "%s: One occurence of '--read' ('-r') or '--write' ('-w') must be given\n", argv[0]);
		return EXIT_FAILURE;
	}

	if(args_info.write_given)
		direction = PCI_WRITE;
	if(args_info.read_given)
		direction = PCI_READ;

	if(args_info.verbose_given)
		verbose = 1;

	if(args_info.file_given)
		file = args_info.file_arg;

	target_addr = string_to_size(args_info.address_arg, argv[0]);
	xfer_size = string_to_size(args_info.size_arg, argv[0]);

	if (!file) {
		use_standard_io = true;
		if (direction == PCI_READ) {
			file = "stdout";
			file_fd = STDOUT_FILENO;
		} else {
			file = "stdin";
			file_fd = STDIN_FILENO;
		}
	}

	access_size = args_info.access_size_arg;
	if (access_size != 1 && access_size != 2 && access_size != 4 && access_size != 8) {
		fprintf(stderr, "Invalid access size, must be 1, 2, 4 or 8\n");
		exit(1);
	}

	if (xfer_size % access_size != 0) {
		fprintf(stderr, "Transfer size must be a multiple of access size\n");
		exit(1);
	}

	fprintf(stderr, "Starting transfer on PCI device %s, resource %d:\n", args_info.device_arg, args_info.resource_arg);
	fprintf(stderr, "\tAddress: 0x%lx\n", target_addr);
	fprintf(stderr, "\tSize: %zd bytes\n", xfer_size);
	fprintf(stderr, "\tAccess size: %zd bytes\n", access_size);
	fprintf(stderr, "\tFile: %s\n", file);
	fprintf(stderr, "\t%s\n", direction == PCI_READ ? "PCI -> HOST" : "HOST -> PCI");

	snprintf(pci_resource_file, 1024, PCI_RESOURCE_FMT, args_info.device_arg, args_info.resource_arg);

	stat(pci_resource_file, &st);
	length = st.st_size;

	pci_resource_fd = open(pci_resource_file, O_RDWR | O_SYNC);
	if (pci_resource_fd < 0) {
		fprintf(stderr, "Failed to open input file %s: %s\n"
			, pci_resource_file, strerror(errno));
		exit(1);
	}

	if(!use_standard_io) {
		if (direction == PCI_READ)  {
			file_fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		} else {
			file_fd = open(file, O_RDONLY);
		}

		if (file_fd < 0) {
			fprintf(stderr, "Failed to open output file %s, %s\n", file, strerror(errno));
			exit(1);
		}
	}

	if (direction == PCI_READ) {
		pcimem_do_read(pci_resource_fd, target_addr, file_fd);
	} else {
		pcimem_do_write(pci_resource_fd, target_addr, file_fd);
	}

	close(file_fd);

	return 0;
}
