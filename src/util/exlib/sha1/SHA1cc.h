/* SHA1cc.h */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* */

/* SHA1_Context */
struct SHA1_Context {

	UINT8  Block[0x40];
	UINT32 State[5 + 1];
	UINT64 Count;

}; /* SHA1_Context */

typedef struct SHA1_Context SHA1_Context_t;

void SHA1cc_Init(SHA1_Context_t* t);
void SHA1cc_Update(SHA1_Context_t* t, const VOID* pv, SIZE_T cb);
void SHA1cc_Finalize(SHA1_Context_t* t, UINT8 digest[20]);

/* */

#ifdef __cplusplus
}
#endif /* __cplusplus */

