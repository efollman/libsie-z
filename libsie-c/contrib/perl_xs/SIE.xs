#include "sie_internal.h"

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

typedef sie_Context * SoMat_SIE;
typedef sie_Object * SoMat_SIE_Object;
typedef sie_Ref * SoMat_SIE_Ref;
typedef sie_Intake * SoMat_SIE_Intake;
typedef sie_File * SoMat_SIE_File;
typedef sie_Stream * SoMat_SIE_Stream;
typedef sie_Iterator * SoMat_SIE_Iterator;
typedef sie_Dimension * SoMat_SIE_Dimension;
typedef sie_Tag * SoMat_SIE_Tag;
typedef sie_Channel * SoMat_SIE_Channel;
typedef sie_Test * SoMat_SIE_Test;
typedef sie_Spigot * SoMat_SIE_Spigot;
typedef sie_Output * SoMat_SIE_Output;
typedef sie_Plot_Crusher * SoMat_SIE_PlotCrusher;
typedef sie_Writer * SoMat_SIE_Writer;
typedef struct {
    SV *self;
    sie_Sifter *p;
    SV *write_fn_ref;
    SV *test_sig_fn_ref;
} *SoMat_SIE_Sifter;

static size_t perl_write_fn(void *user, const char *data, size_t len)
{
    dSP;
    SV *ref = user;
    int count;
    size_t retval;

    ENTER;
    SAVETMPS;

    PUSHMARK(SP);
    XPUSHs(sv_2mortal(newSVpvn(data, len)));
    PUTBACK;

    count = call_sv(ref, G_SCALAR);

    SPAGAIN;

    if (count != 1)
        croak("Big trouble\n");

    retval = POPi;

    PUTBACK;
    FREETMPS;
    LEAVE;

    return retval;
}

static void perl_test_sig_fn(sie_Sifter *ign, sie_Test *test, void *user)
{
    dSP;
    SoMat_SIE_Sifter sifter = user;
    int count;
    SV *self_ref;

    ENTER;
    SAVETMPS;

    PUSHMARK(SP);
    self_ref = newRV_inc(sifter->self);
    sv_bless(self_ref, gv_stashpv("SoMat::SIE::Sifter", 0));
    XPUSHs(sv_2mortal(self_ref));
    XPUSHs(sv_setref_pv(sv_newmortal(), "SoMat::SIE::Test", sie_retain(test)));
    PUTBACK;

    count = call_sv(sifter->test_sig_fn_ref, G_SCALAR);

    SPAGAIN;

    if (count != 1)
        croak("Big trouble\n");

    PUTBACK;
    FREETMPS;
    LEAVE;
}


MODULE = SoMat::SIE     PACKAGE = SoMat::SIE        PREFIX = sie_

SoMat_SIE
new(CLASS)
        char *CLASS
    CODE:
        RETVAL = sie_context_new();
    OUTPUT:
        RETVAL


#/* KLUDGE handle refcounting!!! */
void
DESTROY(ctx)
        SoMat_SIE ctx
    PREINIT:
        int unreleased;
    CODE:
        unreleased = sie_context_done(ctx);
/* printf("SIE Context DESTROY, unreleased=%d\n", unreleased); */
        if (unreleased)
            warn("SoMat::SIE::DESTROY: Memory leak");


SoMat_SIE_File
file_open(ctx, filename)
        SoMat_SIE ctx
        char *filename
    CODE:
        RETVAL = sie_file_open(ctx, filename);
    OUTPUT:
        RETVAL

SoMat_SIE_Writer
new_writer(ctx, write_fn)
        SoMat_SIE ctx
        SV *write_fn
    CODE:
        RETVAL = sie_writer_new(ctx, perl_write_fn, newSVsv(write_fn));
    OUTPUT:
        RETVAL

SoMat_SIE_Stream
new_stream(ctx)
        SoMat_SIE ctx
    CODE:
        RETVAL = sie_stream_new(ctx);
    OUTPUT:
        RETVAL


MODULE = SoMat::SIE::Object     PACKAGE = SoMat::SIE::Object

#/* KLUDGE handle refcounting!!! */
void
DESTROY(obj)
        SoMat_SIE_Object obj
    CODE:
/* printf("SIE Object DESTROY 0x%X (%s) (refcount %d)\n", obj, sie_object_class_name(obj), sie_refcount(obj)); */
        sie_release(obj);

char *
class_name(obj)
        SoMat_SIE_Object obj
    CODE:
        RETVAL = sie_object_class_name(obj);
    OUTPUT:
        RETVAL


MODULE = SoMat::SIE::Ref     PACKAGE = SoMat::SIE::Ref

SoMat_SIE_Iterator
get_channels(ref)
        SoMat_SIE_Ref ref
    PREINIT:
        SoMat_SIE_Iterator chs;
    CODE:
        chs = sie_get_channels(ref);
/* printf("get_channels(0x%X) = 0x%X\n", ref, chs); */
        RETVAL = chs;
    OUTPUT:
        RETVAL

SoMat_SIE_Iterator
get_tests(ref)
        SoMat_SIE_Ref ref
    PREINIT:
        SoMat_SIE_Iterator tests;
    CODE:
        tests = sie_get_tests(ref);
/* printf("get_tests(0x%X) = 0x%X\n", ref, tests); */
        RETVAL = tests;
    OUTPUT:
        RETVAL

SoMat_SIE_Channel
get_channel(ref, id)
        SoMat_SIE_Ref ref
        unsigned long id
    CODE:
        RETVAL = sie_get_channel(ref, id);
    OUTPUT:
        RETVAL

SoMat_SIE_Test
get_test(ref, id)
        SoMat_SIE_Ref ref
        unsigned long id
    CODE:
        RETVAL = sie_get_test(ref, id);
    OUTPUT:
        RETVAL

unsigned long
get_id(ref)
        SoMat_SIE_Ref ref
    CODE:
        RETVAL = sie_get_id(ref);
    OUTPUT:
        RETVAL

int
get_index(ref)
        SoMat_SIE_Ref ref
    CODE:
        RETVAL = sie_get_index(ref);
    OUTPUT:
        RETVAL

SoMat_SIE_Iterator
get_tags(ref)
        SoMat_SIE_Ref ref
    PREINIT:
        SoMat_SIE_Iterator tags;
    CODE:
        tags = sie_get_tags(ref);
/* printf("%s get_tags(ref=0x%X) = 0x%X\n", sie_object_class_name(ref), ref, tags); */
        RETVAL = tags;
    OUTPUT:
        RETVAL

SoMat_SIE_Tag
get_tag(ref, id)
        SoMat_SIE_Ref ref
        char *id
    CODE:
        RETVAL = sie_get_tag(ref, id);
    OUTPUT:
        RETVAL

HV *
tags(ref)
        SoMat_SIE_Ref ref
    PREINIT:
        sie_Iterator *iter;
        HV *hv = NULL;
    CODE:
        iter = sie_get_tags(ref);
        if (iter) {
            sie_Tag *tag;
            hv = newHV();
            while ((tag = sie_iterator_next(iter))) {
                const char *tid = sie_tag_get_id(tag);
                char *tval = NULL;
                size_t tsize;
                sie_tag_get_value_b(tag, &tval, &tsize);
                hv_store(hv, tid, strlen(tid), newSVpv(tval, tsize), 0);
                sie_free(tval);
            }
            sie_release(iter);
        }
        RETVAL = hv;
        if (hv)
            sv_2mortal((SV *)RETVAL);
    OUTPUT:
        RETVAL
        
SoMat_SIE_Spigot
attach_spigot(ref)
        SoMat_SIE_Ref ref
    PREINIT:
        SoMat_SIE_Spigot spigot;
    CODE:
        spigot = sie_attach_spigot(ref);
/* printf("attach_spigot(0x%X) = 0x%X\n", ref, spigot); */
        RETVAL = spigot;
    OUTPUT:
        RETVAL


MODULE = SoMat::SIE::Iterator     PACKAGE = SoMat::SIE::Iterator

SoMat_SIE_Object
_get_next(iter)
        SoMat_SIE_Iterator iter
    PREINIT:
        SoMat_SIE_Object obj;
    CODE:
        obj = sie_iterator_next(iter);
        if (obj)
            sie_retain(obj);
/* printf("Iterator get_next(0x%X) = 0x%X (%s)\n", iter, obj, (obj ? sie_object_class_name(obj) : "NULL")); */
        RETVAL = obj;
    OUTPUT:
        RETVAL


MODULE = SoMat::SIE::Intake     PACKAGE = SoMat::SIE::Intake

SoMat_SIE_Channel
get_channel_from_id(fh, id)
        SoMat_SIE_Intake fh
        int id
    PREINIT:
        SoMat_SIE_Channel ch;
    CODE:
        ch = sie_get_channel(fh, id);
        RETVAL = ch;
    OUTPUT:
        RETVAL


SV *
add_stream_data(intake, data)
        SoMat_SIE_Intake intake
        SV *data
    PREINIT:
        size_t out;
        char *ptr;
        STRLEN len;
    CODE:
        ST(0) = sv_newmortal();
        ptr = SvPV(data, len);
        out = sie_add_stream_data(intake, ptr, len);
        if (out == len)
            sv_setiv(ST(0), 1);


MODULE = SoMat::SIE::Tag     PACKAGE = SoMat::SIE::Tag

void
dump(tag)
        SoMat_SIE_Tag tag
    PREINIT:
        const char *tid;
        char *tval = NULL;
        size_t tsize = 0;
    CODE:
        tid = sie_tag_get_id(tag);
        sie_tag_get_value_b(tag, &tval, &tsize);
/* printf("tag dump(0x%X) %s => %.*s\n", tag, tid, tsize, tval); */
        sie_free(tval);

char *
name(tag)
        SoMat_SIE_Tag tag
    PREINIT:
        const char *tid;
    CODE:
        tid = sie_tag_get_id(tag);
        RETVAL = (char *)tid;
    OUTPUT:
        RETVAL

SV *
value(tag)
        SoMat_SIE_Tag tag
    PREINIT:
        char *tval = NULL;
        size_t tsize = 0;
    PPCODE:
        sie_tag_get_value_b(tag, &tval, &tsize);
        ST(0) = newSVpv(tval, tsize);
        sie_free(tval);
        sv_2mortal(ST(0));
        XSRETURN(1);

SV *
pair(tag)
        SoMat_SIE_Tag tag
    PREINIT:
        const char *tid;
        char *tval = NULL;
        size_t tsize = 0;
    PPCODE:
        tid = sie_tag_get_id(tag);
        ST(0) = newSVpv(tid, 0);
        sv_2mortal(ST(0));
        sie_tag_get_value_b(tag, &tval, &tsize);
        ST(1) = newSVpv(tval, tsize);
        sie_free(tval);
        sv_2mortal(ST(1));
        XSRETURN(2);


MODULE = SoMat::SIE::Channel     PACKAGE = SoMat::SIE::Channel

char *
get_name(ch)
        SoMat_SIE_Channel ch
    CODE:
        RETVAL = (char *)sie_get_name(ch);
    OUTPUT:
        RETVAL

SoMat_SIE_Iterator
get_dimensions(ch)
        SoMat_SIE_Channel ch
    PREINIT:
        SoMat_SIE_Iterator dims;
    CODE:
        dims = sie_get_dimensions(ch);
        RETVAL = dims;
    OUTPUT:
        RETVAL

SoMat_SIE_Dimension
get_dimension(ch, id)
        SoMat_SIE_Channel ch
        unsigned long id
    CODE:
        RETVAL = sie_get_dimension(ch, id);
    OUTPUT:
        RETVAL

SoMat_SIE_Test
get_containing_test(ch)
        SoMat_SIE_Channel ch
    CODE:
        RETVAL = sie_get_containing_test(ch);
    OUTPUT:
        RETVAL

MODULE = SoMat::SIE::Dimension     PACKAGE = SoMat::SIE::Dimension

unsigned long
get_index(dim)
        SoMat_SIE_Dimension dim
    PREINIT:
        unsigned long idx;
    CODE:
        idx = sie_get_index(dim);
        RETVAL = idx;
    OUTPUT:
        RETVAL

MODULE = SoMat::SIE::Spigot     PACKAGE = SoMat::SIE::Spigot

double
seek(spigot, dpos)
        SoMat_SIE_Spigot spigot
        double dpos
    PREINIT:
        size_t pos = dpos;
        size_t ret;
    CODE:
        if (pos < 0)
            pos = SIE_SPIGOT_SEEK_END;
        ret = sie_spigot_seek(spigot, pos);
        if (ret == SIE_SPIGOT_SEEK_END)
            RETVAL = -1.0;
        else
            RETVAL = (double)ret;
    OUTPUT:
        RETVAL

double
tell(spigot)
        SoMat_SIE_Spigot spigot
    PREINIT:
        size_t ret;
    CODE:
        ret = sie_spigot_tell(spigot);
        if (ret == SIE_SPIGOT_SEEK_END)
            RETVAL = -1.0;
        else
            RETVAL = (double)ret;
    OUTPUT:
        RETVAL

SV *
lower_bound(spigot, dim, value)
        SoMat_SIE_Spigot spigot
        size_t dim
        double value
    PREINIT:
        size_t block;
        size_t scan;
    PPCODE:
        if (sie_lower_bound(spigot, dim, value, &block, &scan)) {
            ST(0) = newSVnv(block);
            sv_2mortal(ST(0));
            ST(1) = newSVnv(scan);
            sv_2mortal(ST(1));
            XSRETURN(2);
        } else {
            XSRETURN(0);
        }

SV *
upper_bound(spigot, dim, value)
        SoMat_SIE_Spigot spigot
        size_t dim
        double value
    PREINIT:
        size_t block;
        size_t scan;
    PPCODE:
        if (sie_upper_bound(spigot, dim, value, &block, &scan)) {
            ST(0) = newSVnv(block);
            sv_2mortal(ST(0));
            ST(1) = newSVnv(scan);
            sv_2mortal(ST(1));
            XSRETURN(2);
        } else {
            XSRETURN(0);
        }

SoMat_SIE_Output
get_output(spigot)
        SoMat_SIE_Spigot spigot
    PREINIT:
        SoMat_SIE_Output out;
    CODE:
        out = sie_spigot_get(spigot);
/* printf("get_output(0x%X) = 0x%X\n", spigot, out); */
        RETVAL = sie_retain(out);
    OUTPUT:
        RETVAL

int
done(spigot)
        SoMat_SIE_Spigot spigot
    CODE:
        RETVAL = sie_spigot_done(spigot);
    OUTPUT:
        RETVAL

void
disable_transforms(spigot, disable)
        SoMat_SIE_Spigot spigot
        int disable
    CODE:
        sie_spigot_disable_transforms(spigot, disable);

void
transform_output(spigot, output)
        SoMat_SIE_Spigot spigot
        SoMat_SIE_Output output
    CODE:
        sie_spigot_transform_output(spigot, output);


MODULE = SoMat::SIE::Output     PACKAGE = SoMat::SIE::Output

int
num_vs(out)
        SoMat_SIE_Output out
    CODE:
        RETVAL = out->num_vs;
    OUTPUT:
        RETVAL

int
num_scans(out)
        SoMat_SIE_Output out
    CODE:
        RETVAL = out->num_scans;
    OUTPUT:
        RETVAL

int
num_dims(out)
        SoMat_SIE_Output out
    CODE:
        RETVAL = sie_output_get_num_dims(out);
    OUTPUT:
        RETVAL

int
num_rows(out)
        SoMat_SIE_Output out
    CODE:
        RETVAL = sie_output_get_num_rows(out);
    OUTPUT:
        RETVAL

int
get_type(out, v_idx, scan_idx)
        SoMat_SIE_Output out
        int v_idx
        int scan_idx
    PREINIT:
        sie_Output_V *v;
    CODE:
        v = out->v + v_idx;
/* printf("get_type(0x%X, v=%d, scan=%d), type=%d, size=%d\n",
out, v_idx, scan_idx, v->type, v->size); */
        RETVAL = v->type;
    OUTPUT:
        RETVAL

SV *
scan(out, scan_idx)
        SoMat_SIE_Output out
        int scan_idx
    PREINIT:
        int v;
    PPCODE:
        if (scan_idx < 0 || scan_idx >= out->num_scans)
            XSRETURN(0);
        for (v = 0; v < out->num_vs; v++) {
            switch (out->v[v].type) {
            case SIE_OUTPUT_FLOAT64:
                ST(v) = newSVnv(out->v[v].float64[scan_idx]);
                break;
            case SIE_OUTPUT_RAW:
                ST(v) = newSVpv(out->v[v].raw[scan_idx].ptr,
                    out->v[v].raw[scan_idx].size);
                break;
            default:
                abort(); /* KLUDGE */
            }
            sv_2mortal(ST(v));
        }
        XSRETURN(out->num_vs);

SV *
get_float64_v(out, v)
        SoMat_SIE_Output out
        int v
    CODE:
        if (out->v[v].type != SIE_OUTPUT_FLOAT64)
            RETVAL = NULL;
        else
            RETVAL = newSVpvn((char *)out->v[v].float64,
                out->v_guts[v].size * sizeof(sie_float64));
    OUTPUT:
        RETVAL

MODULE = SoMat::SIE::PlotCrusher     PACKAGE = SoMat::SIE::PlotCrusher


SoMat_SIE_PlotCrusher
new(ignore_type, spigot, scans)
        SV *ignore_type
        SoMat_SIE_Spigot spigot
        size_t scans
    CODE:
        RETVAL = sie_plot_crusher_new(spigot, scans);
    OUTPUT:
        RETVAL

int
work(plot_crusher)
        SoMat_SIE_PlotCrusher plot_crusher
    CODE:
        RETVAL = sie_plot_crusher_work(plot_crusher);
    OUTPUT:
        RETVAL

SoMat_SIE_Output
output(plot_crusher)
        SoMat_SIE_PlotCrusher plot_crusher
    CODE:
        RETVAL = sie_plot_crusher_output(plot_crusher);
    OUTPUT:
        RETVAL

SoMat_SIE_Output
finish(plot_crusher)
        SoMat_SIE_PlotCrusher plot_crusher
    CODE:
        RETVAL = sie_plot_crusher_finish(plot_crusher);
    OUTPUT:
        RETVAL

MODULE = SoMat::SIE::Writer     PACKAGE = SoMat::SIE::Writer

void
DESTROY(writer)
        SoMat_SIE_Writer writer
    PREINIT:
        SV *write_fn;
    CODE:
/* printf("SIE Writer DESTROY 0x%X (%s) (refcount %d)\n", writer, sie_object_class_name(writer), sie_refcount(writer)); */
        write_fn = writer->user;
        sie_release(writer);
        SvREFCNT_dec(write_fn);

int
flush_xml(writer)
        SoMat_SIE_Writer writer
    CODE:
        RETVAL = sie_writer_flush_xml(writer);
    OUTPUT:
        RETVAL

int
xml_string(writer, string)
        SoMat_SIE_Writer writer
        SV *string
    PREINIT:
        char *ptr;
        STRLEN len;
    CODE:
        ptr = SvPV(string, len);
        RETVAL = sie_writer_xml_string(writer, ptr, len);
    OUTPUT:
        RETVAL

int
xml_header(writer)
        SoMat_SIE_Writer writer
    CODE:
        RETVAL = sie_writer_xml_header(writer);
    OUTPUT:
        RETVAL

int
write_block(writer, group, payload)
        SoMat_SIE_Writer writer
        size_t group
        SV *payload
    PREINIT:
        char *ptr;
        STRLEN len;
    CODE:
        ptr = SvPV(payload, len);
        RETVAL = sie_writer_write_block(writer, group, ptr, len);
    OUTPUT:
        RETVAL

SoMat_SIE_Sifter
new_sifter(writer)
        SoMat_SIE_Writer writer
    CODE:
        RETVAL = calloc(1, sizeof(*RETVAL));
        RETVAL->p = sie_sifter_new(writer);
        RETVAL->write_fn_ref = newSVsv(ST(0));
    OUTPUT:
        RETVAL


MODULE = SoMat::SIE::Sifter     PACKAGE = SoMat::SIE::Sifter

void
DESTROY(sifter)
        SoMat_SIE_Sifter sifter
    CODE:
/* printf("SIE Sifter DESTROY 0x%X (%s) (refcount %d)\n", sifter->p, sie_object_class_name(sifter->p), sie_refcount(sifter->p)); */
        sie_release(sifter->p);
        if (sifter->test_sig_fn_ref)
            SvREFCNT_dec(sifter->test_sig_fn_ref);
        SvREFCNT_dec(sifter->write_fn_ref);
        free(sifter);

int
add(sifter, ref)
        SoMat_SIE_Sifter sifter
        SoMat_SIE_Ref ref
    CODE:
        RETVAL = sie_sifter_add(sifter->p, ref);
    OUTPUT:
        RETVAL

int
add_channel(sifter, ref, start_block, end_block)
        SoMat_SIE_Sifter sifter
        SoMat_SIE_Ref ref
        size_t start_block
        size_t end_block
    CODE:
        RETVAL = sie_sifter_add_channel(sifter->p, ref, start_block, end_block);
    OUTPUT:
        RETVAL

void
finish(sifter)
        SoMat_SIE_Sifter sifter
    CODE:
        sie_sifter_finish(sifter->p);

void
test_sig_fn(sifter, fn)
        SoMat_SIE_Sifter sifter
        SV *fn
    CODE:
        if (!sifter->self)
            sifter->self = SvRV(ST(0));
        if (sifter->test_sig_fn_ref)
            SvREFCNT_dec(sifter->test_sig_fn_ref);
        if (SvOK(fn)) {
            sifter->test_sig_fn_ref = newSVsv(fn);
            sie_sifter_test_sig_fn(sifter->p, perl_test_sig_fn, sifter);
        } else {
            sifter->test_sig_fn_ref = NULL;
            sie_sifter_test_sig_fn(sifter->p, NULL, NULL);
        }

void
register_test(sifter, test, key)
        SoMat_SIE_Sifter sifter
        SoMat_SIE_Test test
        SV *key
    PREINIT:
        char *ptr;
        STRLEN len;
    CODE:
        ptr = SvPV(key, len);
        sie_sifter_register_test(sifter->p, test, ptr, len);

double
total_size(sifter)
        SoMat_SIE_Sifter sifter
    CODE:
        RETVAL = sie_sifter_total_size(sifter->p);
    OUTPUT:
        RETVAL


MODULE = SoMat::SIE     PACKAGE = SoMat::SIE        PREFIX = sie_
