// SPDX-License-Identifier: GPL-2.0+
#ifndef TEST_HISI_SEC_H_
#define TEST_HISI_SEC_H
enum cipher_op_type {
	ENCRYPTION,
	DECRYPTION,
};
enum cipher_alg {
	CIPHER_SM4,
	CIPHER_AES,
	CIPHER_DES,
	CIPHER_3DES,
};

enum cipher_mode {
	ECB,
	CBC,
	CTR,
	XTS,
};

struct cipher_testvec {
	const char *key;
	int klen;
	const chat *iv;
	int ivlen;
	const char *iv_out;
	const char *ptext;
	const char *ctext;
	int len;
}


#endif /* TEST_HISI_SEC_H_ */
