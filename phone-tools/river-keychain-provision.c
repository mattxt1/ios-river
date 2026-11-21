/* Provision the persistent device-local wrapping seed consumed by River AKS. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char seed_name[] = "river-wrap-v1";
enum { SEED_BYTES = 32 };

static int
check_seed(int directory)
{
	struct stat attributes;
	if (fstatat(directory, seed_name, &attributes, AT_SYMLINK_NOFOLLOW) != 0) {
		return errno;
	}
	if (!S_ISREG(attributes.st_mode) || (attributes.st_mode & 07777) != 0400 ||
	    attributes.st_uid != 0 || attributes.st_nlink != 1 || attributes.st_size != SEED_BYTES) {
		return EINVAL;
	}
	return 0;
}

static int
provision(int directory)
{
	int error = check_seed(directory);
	if (error != ENOENT) {
		return error;
	}
	char temporary[48];
	snprintf(temporary, sizeof(temporary), ".river-wrap-%ld-%08x", (long)getpid(), arc4random());
	int descriptor = openat(directory, temporary, O_CREAT | O_EXCL | O_NOFOLLOW | O_WRONLY, 0600);
	if (descriptor < 0) {
		return errno;
	}
	uint8_t seed[SEED_BYTES];
	arc4random_buf(seed, sizeof(seed));
	ssize_t written = write(descriptor, seed, sizeof(seed));
	error = written == sizeof(seed) ? 0 : (written < 0 ? errno : EIO);
	memset_s(seed, sizeof(seed), 0, sizeof(seed));
	if (error == 0 && (fchmod(descriptor, 0400) != 0 || fsync(descriptor) != 0)) {
		error = errno;
	}
	if (close(descriptor) != 0 && error == 0) {
		error = errno;
	}
	if (error == 0 && linkat(directory, temporary, directory, seed_name, 0) != 0 && errno != EEXIST) {
		error = errno;
	}
	if (unlinkat(directory, temporary, 0) != 0 && error == 0) {
		error = errno;
	}
	if (error == 0 && fsync(directory) != 0) {
		error = errno;
	}
	return error ? error : check_seed(directory);
}

int
main(void)
{
	if (geteuid() != 0) {
		fputs("River keychain provisioning requires root\n", stderr);
		return 1;
	}
	int directory = open("/private/var/keybags", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
	if (directory < 0) {
		perror("Open keybag directory");
		return 1;
	}
	struct stat attributes;
	if (fstat(directory, &attributes) != 0 || attributes.st_uid != 0 ||
	    (attributes.st_mode & 07777) != 0700) {
		fputs("Unexpected keybag directory ownership or permissions\n", stderr);
		close(directory);
		return 1;
	}
	int error = provision(directory);
	close(directory);
	if (error != 0) {
		fprintf(stderr, "River keychain provisioning failed (%d)\n", error);
		return 1;
	}
	puts("River keychain wrapping seed present");
	return 0;
}
