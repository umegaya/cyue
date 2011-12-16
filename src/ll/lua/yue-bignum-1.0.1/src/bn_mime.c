#include <openssl/bn.h>
#include <stdlib.h>
#include <string.h>
//#include <stdio.h> //DEBUG


/*
 * Adaptado de http://base64.sourceforge.net/b64.c
 */


static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


unsigned int bigendian(char *resp, const BIGNUM *a) {
	// Declaração
	BIGNUM *aux = BN_new();
	BIGNUM *zero = BN_new();
	BIGNUM *one = BN_new();
	BIGNUM *modulus = BN_new();
	BN_CTX *ctx = BN_CTX_new();
	int len = BN_num_bytes(a);
	int i = len;

	// Inicialização
	BN_init(aux);
	BN_init(zero);
	BN_init(one);
	BN_init(modulus);

	BN_copy(aux, a);
	BN_dec2bn(&modulus, "256");
	BN_one(one);

	while (BN_cmp(aux, zero) > 0) {
		BIGNUM *c_bignum = BN_new();
		char *c_str;
		BN_init(c_bignum);
		BN_nnmod(c_bignum, aux, modulus, ctx);
		c_str = BN_bn2dec(c_bignum);
		resp[--i] = (unsigned char) atoi(c_str);
		BN_div(aux, NULL, aux, modulus, ctx);
		BN_free(c_bignum);
	}

	BN_free(aux);
	BN_free(zero);
	BN_free(one);
	BN_free(modulus);
	BN_CTX_free(ctx);

	return len;
}


unsigned int btwoc(char *out, const BIGNUM *a) {
	char *in = (char *) malloc(sizeof(char) * (BN_num_bytes(a) + 512));
	unsigned int len = bigendian(in, a);
	int i;

	if (in[0] < 0) {
		for(i = 0; i < len; i++)
			out[i + 1] = in[i];
		out[0] = '\0';
		len++;
	} else
		for(i = 0; i < len; i++)
			out[i] = in[i];

	return len;
}


void unbtwoc(BIGNUM *a, const char *in, unsigned int len) {
	int j;
	unsigned int bitpos, i;
	unsigned char v;
	BN_zero(a);

	for (i = 0; i < len; i++) {
		v = in[i];
		bitpos = (len - i) * 8;
		for (j = 7; j >= 0; j--)
			if ((v >> j) % 2)
				BN_set_bit(a, bitpos + j);
	}

	/*** ¡ ¡ ¡  B A C A L H A U  ! ! ! ***/
	BN_rshift(a, a, 8);
}


void b64_encodeblock(unsigned char *out, const unsigned char *in, unsigned int len) {
	out[0] = cb64[ in[0] >> 2 ];
	out[1] = cb64[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
	out[2] = (unsigned char) (len > 1 ? cb64[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ] : '=');
	out[3] = (unsigned char) (len > 2 ? cb64[ in[2] & 0x3f ] : '=');
}


unsigned int b64_decodeblock(unsigned char *out, const unsigned char *in) {
	unsigned int i;
	unsigned char aux[4], j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 64; j++)
			if (in[i] == cb64[j])
				break;
		aux[i] = (j < 64) ? j : 0;
	}

	out[0] = (unsigned char) ((aux[0] << 2) | (aux[1] >> 4));
	out[1] = (unsigned char) ((aux[1] << 4) | (aux[2] >> 2));
	out[2] = (unsigned char) ((aux[2] << 6) | aux[3]);

	if (in[2] == '=')
		return 1;
	else if (in[3] == '=')
		return 2;
	else
		return 3;
}


void base64(char *out, const char *in, unsigned int len) {
	unsigned int i, j;
	out[0] = '\0';

	for(i = 0; i < len; i += 3) {
		char *aux = (char *) malloc(sizeof(char) * 5);
		j = len - i;
		b64_encodeblock(aux, in + i, (j < 4) ? j : 3);
		aux[4] = '\0';
		strcat(out, aux);
	}
}


unsigned int unbase64(char *out, const char *in) {
	unsigned int i, j, k, len = 0;
	unsigned int max = strlen(in);

	for(i = 0, j = 0; i < max; i += 4, j += 3) {
		char *aux = (char *) malloc(sizeof(char) * 4);
		len += b64_decodeblock(aux, in + i);
		for (k = 0; k < 3; k++)
			out[j + k] = aux[k];
	}

	return len;
}
