// Main.cpp

#include <windows.h>

#include <stdio.h>

#include "SHA1cc.h"

int wmain(int argc, LPCWSTR argv[])
{
	if (argc < 2) {
		puts("SHA1cc [file]");
		return -1;
	}

	FILE* fp = 0;
	_wfopen_s(&fp, argv[1], L"rb");
	if (fp == 0) {
		puts("Open Error.");
		return -1;
	}

	const INT32 PAGE_SIZE = 0x1000;
	UINT8 buffer[PAGE_SIZE];

	SHA1_Context_t ctx;
	SHA1cc_Init(&ctx);

	for (; ; ) {
		SIZE_T cb = fread(buffer, 1, PAGE_SIZE, fp);
		if (cb == 0) {
			break;
		}
		SHA1cc_Update(&ctx, buffer, cb);
	}

	fclose(fp);

	UINT8 digest[20];
	SHA1cc_Finalize(&ctx, digest);

	char hash[41];
	for (INT32 i = 0; i < 20; i++) {
		static const char* HEX = "0123456789abcdef";
		INT32 by = digest[i];
		hash[i * 2 + 0] = HEX[by >> 0x4];
		hash[i * 2 + 1] = HEX[by &  0xf];
	}
	hash[40] = '\0';

	wprintf(L"%S *%s\n", hash, argv[1]);

	return 0;
}

