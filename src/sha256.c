/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.com
 */

#include "common.h"
#include "libimobiledevice-glue/sha.h"

#include "fixedint.h"

/* the K array */
static const uint32_t K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

/* Various logical functions */

#define RORc(x, y) \
    ( ((((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)((y)&31)) | \
      ((unsigned long)(x)<<(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)

#define STORE32H(x, y)                                                                    \
    { (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255);   \
      (y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

#define LOAD32H(x, y)                           \
    { x = ((unsigned long)((y)[0] & 255)<<24) | \
          ((unsigned long)((y)[1] & 255)<<16) | \
          ((unsigned long)((y)[2] & 255)<<8)  | \
          ((unsigned long)((y)[3] & 255)); }

#define STORE64H(x, y)                                                                     \
   { (y)[0] = (unsigned char)(((x)>>56)&255); (y)[1] = (unsigned char)(((x)>>48)&255);     \
     (y)[2] = (unsigned char)(((x)>>40)&255); (y)[3] = (unsigned char)(((x)>>32)&255);     \
     (y)[4] = (unsigned char)(((x)>>24)&255); (y)[5] = (unsigned char)(((x)>>16)&255);     \
     (y)[6] = (unsigned char)(((x)>>8)&255); (y)[7] = (unsigned char)((x)&255); }

#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y)) 
#define S(x, n)         RORc((x),(n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))
#ifndef MIN
   #define MIN(x, y) ( ((x)<(y))?(x):(y) )
#endif

/* compress 256-bits */
static int sha256_compress(sha256_context * md, unsigned char *buf)
{
    uint32_t S[8], W[64], t0, t1;
    uint32_t t;
    int i;
    /* copy state into S */
    for (i = 0; i < 8; i++) {
        S[i] = md->state[i];
    }
    /* copy the state into 512-bits into W[0..15] */
    for (i = 0; i < 16; i++) {
        LOAD32H(W[i], buf + (4*i));
    }
    /* fill W[16..63] */
    for (i = 16; i < 64; i++) {
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
    }
    /* Compress */
    #define RND(a,b,c,d,e,f,g,h,i) \
    t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i]; \
    t1 = Sigma0(a) + Maj(a, b, c); \
    d += t0; \
    h  = t0 + t1;
    for (i = 0; i < 64; ++i) {
        RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i);
        t = S[7]; S[7] = S[6]; S[6] = S[5]; S[5] = S[4]; 
        S[4] = S[3]; S[3] = S[2]; S[2] = S[1]; S[1] = S[0]; S[0] = t;
    }
    #undef RND
    /* feedback */
    for (i = 0; i < 8; i++) {
        md->state[i] = md->state[i] + S[i];
    }
    return 0;
}

/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int sha256_init(sha256_context * md)
{
    if (md == NULL) return 1;
    md->curlen = 0;
    md->length = 0;
    md->state[0] = 0x6A09E667UL;
    md->state[1] = 0xBB67AE85UL;
    md->state[2] = 0x3C6EF372UL;
    md->state[3] = 0xA54FF53AUL;
    md->state[4] = 0x510E527FUL;
    md->state[5] = 0x9B05688CUL;
    md->state[6] = 0x1F83D9ABUL;
    md->state[7] = 0x5BE0CD19UL;
    md->num_dwords = 8;
    return 0;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param data   The data to hash
   @param inlen  The length of the data (octets)
   @return 0 if successful
*/
int sha256_update (sha256_context * md, const void *data, size_t inlen)
{
    const unsigned char* in = (const unsigned char*)data;
    size_t n;
    size_t i;
    int           err;
    if (md == NULL) return 1;
    if (in == NULL) return 1;
    if (md->curlen > sizeof(md->buf)) {
       return 1;
    }
    while (inlen > 0) {
        if (md->curlen == 0 && inlen >= 64) {
           if ((err = sha256_compress (md, (unsigned char *)in)) != 0) {
              return err;
           }
           md->length += 64 * 8;
           in             += 64;
           inlen          -= 64;
        } else {
           n = MIN(inlen, (64 - md->curlen));

           for (i = 0; i < n; i++) {
            md->buf[i + md->curlen] = in[i];
           }


           md->curlen += n;
           in             += n;
           inlen          -= n;
           if (md->curlen == 64) {
              if ((err = sha256_compress (md, md->buf)) != 0) {
                 return err;
              }
              md->length += 8*64;
              md->curlen = 0;
           }
       }
    }
    return 0;
}

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (32 bytes)
   @return 0 if successful
*/
int sha256_final(sha256_context * md, unsigned char *out)
{
    int i;
    if (md == NULL) return 1;
    if (out == NULL) return 1;
    if (md->curlen >= sizeof(md->buf)) {
       return 1;
    }
    /* increase the length of the message */
    md->length += md->curlen * 8;
    /* append the '1' bit */
    md->buf[md->curlen++] = (unsigned char)0x80;
    /* if the length is currently above 56 bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    if (md->curlen > 56) {
        while (md->curlen < 64) {
            md->buf[md->curlen++] = (unsigned char)0;
        }
        sha256_compress(md, md->buf);
        md->curlen = 0;
    }
    /* pad upto 56 bytes of zeroes */
    while (md->curlen < 56) {
        md->buf[md->curlen++] = (unsigned char)0;
    }
    /* store length */
    STORE64H(md->length, md->buf+56);
    sha256_compress(md, md->buf);
    /* copy output */
    for (i = 0; i < md->num_dwords; i++) {
        STORE32H(md->state[i], out+(4*i));
    }
    return 0;
}

int sha256(const unsigned char *message, size_t message_len, unsigned char *out)
{
    sha256_context ctx;
    int ret;
    if ((ret = sha256_init(&ctx))) return ret;
    if ((ret = sha256_update(&ctx, message, message_len))) return ret;
    if ((ret = sha256_final(&ctx, out))) return ret;
    return 0;
}

int sha224_init(sha224_context * md) {
    if (md == NULL) return 1;

    md->curlen = 0;
    md->length = 0;
    md->state[0] = 0xc1059ed8UL;
    md->state[1] = 0x367cd507UL;
    md->state[2] = 0x3070dd17UL;
    md->state[3] = 0xf70e5939UL;
    md->state[4] = 0xffc00b31UL;
    md->state[5] = 0x68581511UL;
    md->state[6] = 0x64f98fa7UL;
    md->state[7] = 0xbefa4fa4UL;
    md->num_dwords = 6;

    return 0;
}

int sha224_update(sha224_context * md, const void *data, size_t inlen)
{
    return sha256_update(md, data, inlen);
}

int sha224_final(sha224_context * md, unsigned char* out)
{
    return sha256_final(md, out);
}

int sha224(const unsigned char *message, size_t message_len, unsigned char *out)
{
    sha224_context ctx;
    int ret;
    if ((ret = sha224_init(&ctx))) return ret;
    if ((ret = sha224_update(&ctx, message, message_len))) return ret;
    if ((ret = sha224_final(&ctx, out))) return ret;
    return 0;
}
