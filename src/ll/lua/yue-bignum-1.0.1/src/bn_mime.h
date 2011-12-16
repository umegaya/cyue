#ifndef BN_MIME_H
#define BN_MIME_H

char *bigendian(const BIGNUM *);
unsigned int btwoc(char *, const BIGNUM *);
void unbtwoc(BIGNUM *, const char *, unsigned int);
void base64(char *, const char *, unsigned int);
unsigned int unbase64(char *, const char *);

#endif
