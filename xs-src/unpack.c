#define NEED_newRV_noinc
#define NEED_sv_2pv_flags
#define NEED_my_snprintf
#include "xshelper.h"

#define MY_CXT_KEY "Data::MessagePack::_unpack_guts" XS_VERSION
typedef struct {
    SV* msgpack_true;
    SV* msgpack_false;
} my_cxt_t;
START_MY_CXT

typedef struct {
    bool finished;
    bool incremented;
    bool utf8;
} unpack_user;

#include "msgpack/unpack_define.h"

#define msgpack_unpack_struct(name) \
    struct template ## name

#define msgpack_unpack_func(ret, name) \
    STATIC_INLINE ret template ## name

#define msgpack_unpack_callback(name) \
    template_callback ## name

#define msgpack_unpack_object SV*

#define msgpack_unpack_user unpack_user

void init_Data__MessagePack_unpack(pTHX_ bool const cloning) {
    // booleans are load on demand (lazy load).
    if(!cloning) {
        MY_CXT_INIT;
        MY_CXT.msgpack_true  = NULL;
        MY_CXT.msgpack_false = NULL;
    }
    else {
        MY_CXT_CLONE;
        MY_CXT.msgpack_true  = NULL;
        MY_CXT.msgpack_false = NULL;
    }
}



/* ---------------------------------------------------------------------- */
/* utility functions                                                      */

static SV*
load_bool(pTHX_ const char* const name) {
    CV* const cv = get_cv(name, GV_ADD);
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    call_sv((SV*)cv, G_SCALAR);
    SPAGAIN;
    SV* const sv = newSVsv(POPs);
    PUTBACK;
    FREETMPS;
    LEAVE;
    assert(sv);
    assert(sv_isobject(sv));
    return sv;
}

static SV*
get_bool(bool const value) {
    dTHX;
    dMY_CXT;
    if(value) {
        if(!MY_CXT.msgpack_true) {
            MY_CXT.msgpack_true = load_bool(aTHX_ "Data::MessagePack::true");
        }
        return newSVsv(MY_CXT.msgpack_true);
    }
    else {
        if(!MY_CXT.msgpack_false) {
            MY_CXT.msgpack_false = load_bool(aTHX_ "Data::MessagePack::false");
        }
        return newSVsv(MY_CXT.msgpack_false);
    }
}

/* ---------------------------------------------------------------------- */
struct template_context;
typedef struct template_context msgpack_unpack_t;

static void template_init(msgpack_unpack_t* u);

static SV* template_data(msgpack_unpack_t* u);

static int template_execute(msgpack_unpack_t* u PERL_UNUSED_DECL,
    const char* data, size_t len, size_t* off);

STATIC_INLINE SV* template_callback_root(unpack_user* u PERL_UNUSED_DECL)
{
    return NULL;
}

#if IVSIZE == 4

STATIC_INLINE int template_callback_UV(unpack_user* u PERL_UNUSED_DECL, UV const d, SV** o)
{
    dTHX;
    *o = newSVuv(d);
    return 0;
}

STATIC_INLINE int template_callback_IV(unpack_user* u PERL_UNUSED_DECL, IV const d, SV** o)
{
    dTHX;
    *o = newSViv(d);
    return 0;
}

static int template_callback_uint64(unpack_user* u PERL_UNUSED_DECL, uint64_t const d, SV** o)
{
    dTHX;
    char tbuf[64];
    STRLEN const len = my_snprintf(tbuf, sizeof(tbuf), "%llu", d);
    *o = newSVpvn(tbuf, len);
    return 0;
}

static int template_callback_int64(unpack_user* u PERL_UNUSED_DECL, int64_t const d, SV** o)
{
    dTHX;
    char tbuf[64];
    STRLEN const len = my_snprintf(tbuf, sizeof(tbuf), "%lld", d);
    *o = newSVpvn(tbuf, len);
    return 0;
}

#else /* IVSIZE == 8 */


STATIC_INLINE int template_callback_UV(unpack_user* u PERL_UNUSED_DECL, UV const d, SV** o)
{
    dTHX;
    *o = newSVuv(d);
    return 0;
}

#define template_callback_uint64 template_callback_UV

STATIC_INLINE int template_callback_IV(unpack_user* u PERL_UNUSED_DECL, IV const d, SV** o)
{
    dTHX;
    *o = newSViv(d);
    return 0;
}

#define template_callback_int64 template_callback_IV

#endif /* IVSIZE */

#define template_callback_uint8  template_callback_UV
#define template_callback_uint16 template_callback_UV
#define template_callback_uint32 template_callback_UV

#define template_callback_int8  template_callback_IV
#define template_callback_int16 template_callback_IV
#define template_callback_int32 template_callback_IV

#define template_callback_float template_callback_double

STATIC_INLINE int template_callback_double(unpack_user* u PERL_UNUSED_DECL, double d, SV** o)
{
    dTHX;
    *o = newSVnv(d);
    return 0;
}

/* &PL_sv_undef is not so good. see http://gist.github.com/387743 */
STATIC_INLINE int template_callback_nil(unpack_user* u PERL_UNUSED_DECL, SV** o)
{
    dTHX;
    *o = newSV(0);
    return 0;
}

STATIC_INLINE int template_callback_true(unpack_user* u PERL_UNUSED_DECL, SV** o)
{
    *o = get_bool(true);
    return 0;
}

STATIC_INLINE int template_callback_false(unpack_user* u PERL_UNUSED_DECL, SV** o)
{
    *o = get_bool(false);
    return 0;
}

STATIC_INLINE int template_callback_array(unpack_user* u PERL_UNUSED_DECL, unsigned int n, SV** o)
{
    dTHX;
    AV* const a = newAV();
    *o = newRV_noinc((SV*)a);
    av_extend(a, n + 1);
    return 0;
}

STATIC_INLINE int template_callback_array_item(unpack_user* u PERL_UNUSED_DECL, SV** c, SV* o)
{
    dTHX;
    AV* const a = (AV*)SvRV(*c);
    assert(SvTYPE(a) == SVt_PVAV);
    (void)av_store(a, AvFILLp(a) + 1, o); // the same as av_push(a, o)
    return 0;
}

STATIC_INLINE int template_callback_map(unpack_user* u PERL_UNUSED_DECL, unsigned int n, SV** o)
{
    dTHX;
    HV* const h = newHV();
    hv_ksplit(h, n);
    *o = newRV_noinc((SV*)h);
    return 0;
}

STATIC_INLINE int template_callback_map_item(unpack_user* u PERL_UNUSED_DECL, SV** c, SV* k, SV* v)
{
    dTHX;
    HV* const h = (HV*)SvRV(*c);
    assert(SvTYPE(h) == SVt_PVHV);
    (void)hv_store_ent(h, k, v, 0);
    SvREFCNT_dec(k);
    return 0;
}

STATIC_INLINE int template_callback_raw(unpack_user* u PERL_UNUSED_DECL, const char* b PERL_UNUSED_DECL, const char* p, unsigned int l, SV** o)
{
    dTHX;
    /*  newSVpvn(p, l) returns an undef if p == NULL */
    *o = ((l==0) ? newSVpvs("") : newSVpvn(p, l));
    if(u->utf8) {
        sv_utf8_decode(*o);
    }
    return 0;
}

#include "msgpack/unpack_template.h"

#define UNPACKER(from, name)                                                  \
    msgpack_unpack_t *name;                                                   \
    {                                                                         \
        SV* const obj = from;                                                 \
        if(!(SvROK(obj) && SvIOK(SvRV(obj)))) {                               \
            Perl_croak(aTHX_ "Invalid unpacker instance for " #name);         \
        }                                                                     \
        name = INT2PTR(msgpack_unpack_t*, SvIVX(SvRV((obj))));                \
        if(name == NULL) {                                                    \
            Perl_croak(aTHX_ "NULL found for " # name " when shouldn't be");  \
        }                                                                     \
    }

XS(xs_unpack) {
    dXSARGS;
    SV* const data = ST(1);
    size_t limit;

    if (items == 2) {
        limit = sv_len(data);
    }
    else if(items == 3) {
        limit = SvUVx(ST(2));
    }
    else {
        Perl_croak(aTHX_ "Usage: Data::MessagePack->unpack('data' [, $limit])");
    }

    STRLEN dlen;
    const char* const dptr = SvPV_const(data, dlen);

    msgpack_unpack_t mp;
    template_init(&mp);

    unpack_user const u = {false, false, false};
    mp.user = u;

    size_t from = 0;
    int const ret = template_execute(&mp, dptr, (size_t)dlen, &from);
    SV* const obj = template_data(&mp);
    sv_2mortal(obj);

    if(ret < 0) {
        Perl_croak(aTHX_ "Data::MessagePack->unpack: parse error");
    } else if(ret == 0) {
        Perl_croak(aTHX_ "Data::MessagePack->unpack: insufficient bytes");
    } else {
        if(from < dlen) {
            Perl_croak(aTHX_ "Data::MessagePack->unpack: extra bytes");
        }
    }

    ST(0) = obj;
    XSRETURN(1);
}

/* ------------------------------ stream -- */
/* http://twitter.com/frsyuki/status/13249304748 */

STATIC_INLINE void _reset(SV* const self) {
    dTHX;
	unpack_user const u = {false, false, false};

	UNPACKER(self, mp);
	template_init(mp);
	mp->user = u;
}

XS(xs_unpacker_new) {
    dXSARGS;
    if (items != 1) {
        Perl_croak(aTHX_ "Usage: Data::MessagePack::Unpacker->new()");
    }

    SV* const self = sv_newmortal();
    msgpack_unpack_t *mp;

    Newxz(mp, 1, msgpack_unpack_t);

    sv_setref_pv(self, "Data::MessagePack::Unpacker", mp);
    _reset(self);

    ST(0) = self;
    XSRETURN(1);
}

XS(xs_unpacker_utf8) {
    dXSARGS;
    if (!(items == 1 || items == 2)) {
        Perl_croak(aTHX_ "Usage: $unpacker->utf8([$bool)");
    }
    UNPACKER(ST(0), mp);
    mp->user.utf8 = (items == 1 || sv_true(ST(1))) ? true : false;
    XSRETURN(1); // returns $self
}

XS(xs_unpacker_get_utf8) {
    dXSARGS;
    if (items != 1) {
        Perl_croak(aTHX_ "Usage: $unpacker->get_utf8()");
    }
    UNPACKER(ST(0), mp);
    ST(0) = boolSV(mp->user.utf8);
    XSRETURN(1);
}

STATIC_INLINE size_t
_execute_impl(SV* const self, SV* const data, UV const offset, UV const limit) {
    dTHX;

    if(offset >= limit) {
        Perl_croak(aTHX_ "offset (%"UVuf") is bigger than data buffer size (%"UVuf")",
            offset, limit);
    }

    UNPACKER(self, mp);

    size_t from = offset;
    const char* const dptr = SvPV_nolen_const(data);

    int const ret = template_execute(mp, dptr, limit, &from);

    if(ret < 0) {
        Perl_croak(aTHX_ "Data::MessagePack::Unpacker: parse error while executing");
    }
    mp->user.finished = (ret > 0) ? true : false;
    return from;
}

XS(xs_unpacker_execute) {
    dXSARGS;
    SV* const self = ST(0);
    SV* const data = ST(1);
    UV offset;

    if (items == 2) {
        offset = 0;
    }
    else if (items == 3) {
        offset = SvUVx(ST(2));
    }
    else {
        Perl_croak(aTHX_ "Usage: $unpacker->execute(data, offset = 0)");
    }

    dXSTARG;
    sv_setuv(TARG, _execute_impl(self, data, offset, sv_len(data)));
    ST(0) = TARG;
    XSRETURN(1);
}

XS(xs_unpacker_execute_limit) {
    dXSARGS;
    if (items != 4) {
        Perl_croak(aTHX_ "Usage: $unpacker->execute_limit(data, offset, limit)");
    }

    SV* const self   = ST(0);
    SV* const data   = ST(1);
    UV  const offset = SvUVx(ST(2));
    UV  const limit  = SvUVx(ST(3));

    dXSTARG;
    sv_setuv(TARG, _execute_impl(self, data, offset, limit));
    ST(0) = TARG;
    XSRETURN(1);
}

XS(xs_unpacker_is_finished) {
    dXSARGS;
    if (items != 1) {
        Perl_croak(aTHX_ "Usage: $unpacker->is_finished()");
    }

    UNPACKER(ST(0), mp);
    ST(0) = boolSV(mp->user.finished);
    XSRETURN(1);
}

XS(xs_unpacker_data) {
    dXSARGS;
    if (items != 1) {
        Perl_croak(aTHX_ "Usage: $unpacker->data()");
    }

    UNPACKER(ST(0), mp);
    ST(0) = template_data(mp);
    XSRETURN(1);
}

XS(xs_unpacker_reset) {
    dXSARGS;
    if (items != 1) {
        Perl_croak(aTHX_ "Usage: $unpacker->reset()");
    }

    UNPACKER(ST(0), mp);

    SV* const data = template_data(mp);
    SvREFCNT_dec(data);
    _reset(ST(0));

    XSRETURN(0);
}

XS(xs_unpacker_destroy) {
    dXSARGS;
    if (items != 1) {
        Perl_croak(aTHX_ "Usage: $unpacker->DESTROY()");
    }

    UNPACKER(ST(0), mp);

    SV* const data = template_data(mp);
    SvREFCNT_dec(data);
    Safefree(mp);

    XSRETURN(0);
}
