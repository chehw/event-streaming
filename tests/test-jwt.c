/*
 * test-jwt.c
 * 
 * Copyright 2021 chehw <hongwei.che@gmail.com>
 * 
 * The MIT License
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of 
 * this software and associated documentation files (the "Software"), to deal in 
 * the Software without restriction, including without limitation the rights to 
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
 * of the Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 */

/** 
 * # build: 
 * $ cd ${project_dir}
 * 
 * ## auto generate a RSA key-pair (if not exists) for testing
 * $ tests/make.sh test-jwt
 * 
 * # run:
 * ## (first time) generate a jwt object and output to a file
 * $ tests/test-jwt gen
 * 
 * ## test verify
 * $ tests/test-jwt
 * 
 * # check memory leaks:
 * $ valgrind --leak-check=full tests/test-jwt
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <jwt.h>

/* 
 * JWT Claims
 * https://tools.ietf.org/html/rfc7519#page-8
**/
enum jwt_registered_claim_type
{
	jwt_registered_claim_iss = 0,
	jwt_registered_claim_sub,
	jwt_registered_claim_aud,
	jwt_registered_claim_exp,
	jwt_registered_claim_nbf,
	jwt_registered_claim_iat,
	jwt_registered_claim_jti,
	jwt_registered_claim_types_count
};
static const char * jwt_registered_claims[] = {
	[jwt_registered_claim_iss] = "iss", // (Issuer) Claim
	[jwt_registered_claim_sub] = "sub", // (Subject) Claim
	[jwt_registered_claim_aud] = "aud", // (Audience) Claim
	[jwt_registered_claim_exp] = "exp", // (Expiration Time) Claim
	[jwt_registered_claim_nbf] = "nbf", // (Not Before) Claim
	[jwt_registered_claim_iat] = "iat", // (Issued At) Claim
	[jwt_registered_claim_jti] = "jti", // (JWT ID) Claim
};


#define OUTPUT_JWT_FILE 	"tests/test-response.jwt"
#define SECKEY_FILE 		"seckey.pem"
#define PUBKEY_FILE 		"seckey_pub.pem"

int test_generate(const char * privkey_file);
int test_verify(const char * pubkey_file);

int main(int argc, char **argv)
{
	if(argc > 1) test_generate(NULL); 
	test_verify(NULL);
	return 0;
}

#include <uuid/uuid.h>
#include <sys/stat.h>

#define AUTO_FREE_PTR __attribute__((cleanup(auto_free_ptr)))
static void auto_free_ptr(void * ptr) {
	if(NULL == ptr) return;
	void * p = *(void **)ptr;
	if(p) {
		free(p);
		*(void **)ptr = NULL;
	}
	return;
}

ssize_t load_file_data(const char * filename, unsigned char ** p_data)
{
	assert(filename);
	struct stat st[1];
	memset(st, 0, sizeof(st));
	
	int rc = stat(filename, st);
	if(rc || (st->st_mode & S_IFMT) != S_IFREG) return -1;
	if(NULL == p_data) return (st->st_size + 1);
	
	unsigned char * data = *p_data;
	ssize_t cb_data = 0;
	if(NULL == data) {
		data = calloc(st->st_size + 1, 1);
		assert(data);
		*p_data = data;
	}
	FILE * fp = fopen(filename, "rb");
	assert(fp);
	cb_data = fread(data, 1, st->st_size, fp);
	fclose(fp);
	data[cb_data] = '\0';
	return cb_data;
}

int test_generate(const char * seckey_file)
{
	if(NULL == seckey_file) seckey_file = SECKEY_FILE;
	fprintf(stderr, "==== %s(key=[%s]) ====\n", __FUNCTION__, seckey_file);
	
	jwt_alg_t alg = JWT_ALG_RS256;
	jwt_t * jwt = NULL;
	int ret = jwt_new(&jwt);
	assert(0 == ret);
	
	char id_token[64] = "";
	uuid_t uuid;
	uuid_generate(uuid);
	uuid_unparse(uuid, id_token);
	
	struct timespec issue_at = { 0 };
	clock_gettime(CLOCK_REALTIME, &issue_at);
	
	ret = jwt_add_grant_int(jwt, "iat", (long)issue_at.tv_sec); assert(0 == ret);
	ret = jwt_add_grant(jwt, jwt_registered_claims[jwt_registered_claim_iss], "test::ca"); assert(0 == ret);
	ret = jwt_add_grant(jwt, jwt_registered_claims[jwt_registered_claim_sub], "test-only"); assert(0 == ret);
	ret = jwt_add_grant(jwt, jwt_registered_claims[jwt_registered_claim_jti], id_token); assert(0 == ret);
	
	// add custom claims
	// jwt_add_grant(jwt, claimed_key1, claimed_value1);
	// jwt_add_grant(jwt, claimed_key2, claimed_value2);
	
	AUTO_FREE_PTR unsigned char * seckey = NULL;
	ssize_t cb_seckey = load_file_data(seckey_file, &seckey);
	assert(cb_seckey > 0 && seckey);

	ret = jwt_set_alg(jwt, alg, seckey, cb_seckey);
	assert(0 == ret);
	
	fprintf(stderr, "jwt_response(no signature): \n");
	jwt_dump_fp(jwt, stderr, 1);
	
	FILE * output_fp = fopen(OUTPUT_JWT_FILE, "w+");
	ret = jwt_encode_fp(jwt, output_fp);
	fclose(output_fp);
	assert(0 == ret);
	
	jwt_free(jwt);
	return 0;
}


int test_verify(const char * pubkey_file)
{
	if(NULL == pubkey_file) pubkey_file = PUBKEY_FILE;
	fprintf(stderr, "==== %s(key=[%s]) ====\n", __FUNCTION__, pubkey_file);
	int ret = 0;
	
	AUTO_FREE_PTR unsigned char * pubkey = NULL;
	ssize_t cb_pubkey = load_file_data(pubkey_file, &pubkey);
	assert(cb_pubkey > 0 && pubkey);
	if(pubkey[cb_pubkey - 1] == '\n') pubkey[--cb_pubkey] = '\0';
	
	AUTO_FREE_PTR unsigned char * sz_jwt_data = NULL;
	ssize_t cb_jwt_data = load_file_data(OUTPUT_JWT_FILE, &sz_jwt_data);
	assert(cb_jwt_data > 0 && sz_jwt_data);
	
	printf("pubkey(cb=%Zd): [%s]\n", cb_pubkey, (char *)pubkey);
	
	jwt_t * jwt = NULL;
	ret = jwt_decode(&jwt, (char *)sz_jwt_data, pubkey, cb_pubkey);
	printf("jwt_decode(%s)=%d\n", sz_jwt_data, ret);
	
	if(jwt) {
		fprintf(stderr, "jwt_decoded: \n");
		jwt_dump_fp(jwt, stderr, 1);
	}
	
	jwt_free(jwt);
	return 0;
}
