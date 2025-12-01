#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <stdint.h>

char *md5(const char *str, int length) {
    EVP_MD_CTX *mdctx;
    unsigned int md5_digest_len = EVP_MD_size(EVP_md5());
    uint8_t md5_digest[md5_digest_len];
    char *hexdigest = (char *) malloc(md5_digest_len * 2 + 1);

    // MD5_Init
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_md5(), NULL);

    // MD5_Update
    while (length > 0) {
        if (length > 512) {
            EVP_DigestUpdate(mdctx, str, 512);
        } else {
            EVP_DigestUpdate(mdctx, str, length);
        }
        length -= 512;
        str += 512;
    }

    // MD5_Final
    EVP_DigestFinal_ex(mdctx, md5_digest, &md5_digest_len);
    EVP_MD_CTX_free(mdctx);

    for (int n = 0; n < md5_digest_len; ++n) {
        snprintf(hexdigest + n * 2, 3, "%02x", md5_digest[n]);
    }

    hexdigest[md5_digest_len * 2] = '\0';

    return hexdigest;
}
