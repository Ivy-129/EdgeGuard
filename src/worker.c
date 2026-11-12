#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "worker.h"

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>

/*
 * 判斷 n 是否為質數。
 * 只需要測試到 sqrt(n)，但不使用浮點數。
 */
static int is_prime(uint32_t n) {
    if (n < 2)
        return 0;

    if (n == 2)
        return 1;

    if (n % 2 == 0)
        return 0;

    for (uint32_t d = 3; d <= n / d; d += 2) {
        if (n % d == 0)
            return 0;
    }

    return 1;
}

/*
 * 計算 2 到 limit 之間的質數數量。
 *
 * 注意：
 * 此函式目前不負責 fork()。
 * 未來由 Parent Server 建立獨立 Worker Process。
 */
int worker_run_bench(SharedJobTable *table, uint32_t job_id, uint32_t limit) {
    if (!table || job_id == 0 || limit == 0 || limit > 10000000U)
        return -1;

    uint32_t prime_count = 0;

    for (uint32_t n = 2; n <= limit; ++n) {
        if (is_prime(n))
            ++prime_count;
    }

    char result[512];

    snprintf(result, sizeof(result),
             "TASK=BENCH\n"
             "LIMIT=%" PRIu32 "\n"
             "PRIME_COUNT=%" PRIu32 "\n"
             "WORKER_PID=%ld\n",
             limit, prime_count, (long)getpid());

    return job_finish(table, job_id, JOB_COMPLETED, result);
}

int worker_run_hash(SharedJobTable *table, uint32_t job_id,
                    const char *filename) {
    if (filename == NULL || filename[0] == '\0' || strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0 || strchr(filename, '/') != NULL) {
        job_finish(table, job_id, JOB_FAILED, "Invalid filename\n");
        return -1;
    }

    int dirfd = open("hash_files", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirfd < 0) {
        job_finish(table, job_id, JOB_FAILED, "Cannot open HASH directory\n");
        return -1;
    }

    int fd =
        openat(dirfd, filename, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    close(dirfd);

    if (fd < 0) {
        job_finish(table, job_id, JOB_FAILED, "Cannot open requested file\n");
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        job_finish(table, job_id, JOB_FAILED,
                   "Only regular files are allowed\n");
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        close(fd);
        job_finish(table, job_id, JOB_FAILED,
                   "SHA-256 initialization failed\n");
        return -1;
    }

    int success = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1;
    unsigned char buffer[8192];

    while (success) {
        ssize_t n = read(fd, buffer, sizeof(buffer));

        if (n == 0)
            break;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            success = 0;
            break;
        }

        if (EVP_DigestUpdate(ctx, buffer, (size_t)n) != 1) {
            success = 0;
            break;
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    if (success) {
        success = EVP_DigestFinal_ex(ctx, digest, &digest_len) == 1;
    }

    EVP_MD_CTX_free(ctx);
    close(fd);

    if (!success || digest_len != 32) {
        job_finish(table, job_id, JOB_FAILED, "SHA-256 calculation failed\n");
        return -1;
    }

    char hex[65];

    for (unsigned int i = 0; i < digest_len; ++i) {
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }

    hex[64] = '\0';

    char result[512];
    snprintf(result, sizeof(result),
             "TASK=HASH\nFILE=%s\nSHA256=%s\nWORKER_PID=%ld\n", filename, hex,
             (long)getpid());

    return job_finish(table, job_id, JOB_COMPLETED, result);
}
