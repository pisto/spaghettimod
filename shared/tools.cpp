// implementation of generic tools

#include "cube.h"

////////////////////////// rnd numbers ////////////////////////////////////////

#define N              (624)             
#define M              (397)                
#define K              (0x9908B0DFU)       
#define hiBit(u)       ((u) & 0x80000000U)  
#define loBit(u)       ((u) & 0x00000001U)  
#define loBits(u)      ((u) & 0x7FFFFFFFU)  
#define mixBits(u, v)  (hiBit(u)|loBits(v)) 

static uint state[N+1];     
static uint *next;          
static int left = -1;     

void seedMT(uint seed)
{
    register uint x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
    register int j;
    for(left=0, *s++=x, j=N; --j; *s++ = (x*=69069U) & 0xFFFFFFFFU);
}

uint reloadMT(void)
{
    register uint *p0=state, *p2=state+2, *pM=state+M, s0, s1;
    register int j;
    if(left < -1) seedMT(time(NULL));
    left=N-1, next=state+1;
    for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    for(pM=state, j=M; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1=state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1 ^= (s1 >> 11);
    s1 ^= (s1 <<  7) & 0x9D2C5680U;
    s1 ^= (s1 << 15) & 0xEFC60000U;
    return(s1 ^ (s1 >> 18));
}

uint randomMT(void)
{
    uint y;
    if(--left < 0) return(reloadMT());
    y  = *next++;
    y ^= (y >> 11);
    y ^= (y <<  7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    return(y ^ (y >> 18));
}

///////////////////////// cryptography /////////////////////////////////

#include "crypto.h"

const ecjacobian ecjacobian::origin(gfield((gfield::digit)1), gfield((gfield::digit)1), gfield((gfield::digit)0));

#if GF_BITS==192
const gfield gfield::P("fffffffffffffffffffffffffffffffeffffffffffffffff");
const gfield ecjacobian::B("64210519e59c80e70fa7e9ab72243049feb8deecc146b9b1");
const ecjacobian ecjacobian::base(
    gfield("188da80eb03090f67cbf20eb43a18800f4ff0afd82ff1012"),
    gfield("07192b95ffc8da78631011ed6b24cdd573f977a11e794811")
);
#elif GF_BITS==224
const gfield gfield::P("ffffffffffffffffffffffffffffffff000000000000000000000001");
const gfield ecjacobian::B("b4050a850c04b3abf54132565044b0b7d7bfd8ba270b39432355ffb4");
const ecjacobian ecjacobian::base(
    gfield("b70e0cbd6bb4bf7f321390b94a03c1d356c21122343280d6115c1d21"),
    gfield("bd376388b5f723fb4c22dfe6cd4375a05a07476444d5819985007e34"),
);
#elif GF_BITS==256
const gfield gfield::P("ffffffff00000001000000000000000000000000ffffffffffffffffffffffff");
const gfield ecjacobian::B("5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b");
const ecjacobian ecjacobian::base(
    gfield("6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"),
    gfield("4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5"),
);
#elif GF_BITS==384
const gfield gfield::P("fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff");
const gfield ecjacobian::B("b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef");
const ecjacobian ecjacobian::base(
    gfield("aa87ca22be8b05378eb1c71ef320ad746e1d3b628ba79b9859f741e082542a385502f25dbf55296c3a545e3872760ab7"),
    gfield("3617de4a96262c6f5d9e98bf9292dc29f8f41dbd289a147ce9da3113b5f0b8c00a60b1ce1d7e819d7a431d7c90ea0e5f"),
);
#elif GF_BITS==521
const gfield gfield::P("1ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
const gfield ecjacobian::B("051953eb968e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00");
const ecjacobian ecjacobian::base(
    gfield("c6858e06b70404e9cd9e3ecb662395b4429c648139053fb521f828af606b4d3dbaa14b5e77efe75928fe1dc127a2ffa8de3348b3c1856a429bf97e7e31c2e5bd66"),
    gfield("11839296a789a3bc0045c8a5fb42c7d1bd998f54449579b446817afbd17273e662c97ee72995ef42640c550b9013fad0761353c7086a272c24088be94769fd16650")
);
#else
#error Unsupported GF
#endif

