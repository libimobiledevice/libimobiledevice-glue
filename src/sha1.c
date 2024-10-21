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

#define ROLc(x, y) \
     ( (((unsigned long)(x)<<(unsigned long)((y)&31)) | \
       (((unsigned long)(x)&0xFFFFFFFFUL)>>(unsigned long)(32-((y)&31)))) & 0xFFFFFFFFUL)
#define ROL ROLc

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

#define F0(x,y,z)  (z ^ (x & (y ^ z)))
#define F1(x,y,z)  (x ^ y ^ z)
#define F2(x,y,z)  ((x & y) | (z & (x | y)))
#define F3(x,y,z)  (x ^ y ^ z)
#ifndef MIN
   #define MIN(x, y) ( ((x)<(y))?(x):(y) )
#endif

static int  sha1_compress(sha1_context *md, unsigned char *buf)
{
    uint32_t a,b,c,d,e,W[80],i;
    uint32_t t;
    /* copy the state into 512-bits into W[0..15] */
    for (i = 0; i < 16; i++) {
        LOAD32H(W[i], buf + (4*i));
    }
    /* copy state */
    a = md->state[0];
    b = md->state[1];
    c = md->state[2];
    d = md->state[3];
    e = md->state[4];
    /* expand it */
    for (i = 16; i < 80; i++) {
        W[i] = ROL(W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1); 
    }
    /* compress */
    /* round one */
    #define FF0(a,b,c,d,e,i) e = (ROLc(a, 5) + F0(b,c,d) + e + W[i] + 0x5a827999UL); b = ROLc(b, 30);
    #define FF1(a,b,c,d,e,i) e = (ROLc(a, 5) + F1(b,c,d) + e + W[i] + 0x6ed9eba1UL); b = ROLc(b, 30);
    #define FF2(a,b,c,d,e,i) e = (ROLc(a, 5) + F2(b,c,d) + e + W[i] + 0x8f1bbcdcUL); b = ROLc(b, 30);
    #define FF3(a,b,c,d,e,i) e = (ROLc(a, 5) + F3(b,c,d) + e + W[i] + 0xca62c1d6UL); b = ROLc(b, 30);

    for (i = 0; i < 20; ) {
       FF0(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }
    for (; i < 40; ) {
       FF1(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }
    for (; i < 60; ) {
       FF2(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }
    for (; i < 80; ) {
       FF3(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }

    #undef FF0
    #undef FF1
    #undef FF2
    #undef FF3

    /* store */
    md->state[0] = md->state[0] + a;
    md->state[1] = md->state[1] + b;
    md->state[2] = md->state[2] + c;
    md->state[3] = md->state[3] + d;
    md->state[4] = md->state[4] + e;
    return 0;
}

/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return 0 if successful
*/
int sha1_init(sha1_context * md)
{
    if (md == NULL) return 1;
    md->state[0] = 0x67452301UL;
    md->state[1] = 0xefcdab89UL;
    md->state[2] = 0x98badcfeUL;
    md->state[3] = 0x10325476UL;
    md->state[4] = 0xc3d2e1f0UL;
    md->curlen = 0;
    md->length = 0;
    return 0;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param data   The data to hash
   @param inlen  The length of the data (octets)
   @return 0 if successful
*/
int sha1_update (sha1_context * md, const void *data, size_t inlen)
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
           if ((err = sha1_compress (md, (unsigned char *)in)) != 0) {
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
              if ((err = sha1_compress (md, md->buf)) != 0) {
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
   @param out [out] The destination of the hash (20 bytes)
   @return 0 if successful
*/
int sha1_final(sha1_context * md, unsigned char *out)
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
        sha1_compress(md, md->buf);
        md->curlen = 0;
    }
    /* pad upto 56 bytes of zeroes */
    while (md->curlen < 56) {
        md->buf[md->curlen++] = (unsigned char)0;
    }
    /* store length */
    STORE64H(md->length, md->buf+56);
    sha1_compress(md, md->buf);
    /* copy output */
    for (i = 0; i < 5; i++) {
        STORE32H(md->state[i], out+(4*i));
    }
    return 0;
}

int sha1(const unsigned char *message, size_t message_len, unsigned char *out)
{
    sha1_context ctx;
    int ret;
    if ((ret = sha1_init(&ctx))) return ret;
    if ((ret = sha1_update(&ctx, message, message_len))) return ret;
    if ((ret = sha1_final(&ctx, out))) return ret;
    return 0;
}
