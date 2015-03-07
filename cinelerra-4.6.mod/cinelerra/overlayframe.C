
/*
 * CINELERRA
 * Copyright (C) 2008 Adam Williams <broadcast at earthling dot net>
 * Copyright (C) 2012 Monty <monty@xiph.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "clip.h"
#include "edl.inc"
#include "mutex.h"
#include "overlayframe.h"
#include "units.h"
#include "vframe.h"

static inline int   mabs(int32_t v) { return abs(v); }
static inline int   mabs(int64_t v) { return llabs(v); }
static inline float mabs(float v)   { return fabsf(v); }

static inline int32_t aclip(int32_t v, int mx) {
	return v < 0 ? 0 : v > mx ? mx : v;
}
static inline int64_t aclip(int64_t v, int mx) {
	return v < 0 ? 0 : v > mx ? mx : v;
}
static inline float   aclip(float v, float mx) {
	return v < 0 ? 0 : v > mx ? mx : v;
}
static inline float   aclip(float v, int mx) {
	return v < 0 ? 0 : v > mx ? mx : v;
}
static inline int   aclip(int v, float mx) {
	return v < 0 ? 0 : v > mx ? mx : v;
}
static inline int32_t cclip(int32_t v, int mx) {
	return v > (mx/=2) ? mx : v < (mx=(-mx-1)) ? mx : v;
}
static inline int64_t cclip(int64_t v, int mx) {
	return v > (mx/=2) ? mx : v < (mx=(-mx-1)) ? mx : v;
}
static inline float   cclip(float v, float mx) {
	return v > (mx/=2) ? mx : v < (mx=(-mx)) ? mx : v;
}
static inline float   cclip(float v, int mx) {
	return v > (mx/=2) ? mx : v < (mx=(-mx-1)) ? mx : v;
}
static inline int   cclip(int v, float mx) {
	return v > (mx/=2) ? mx : v < (mx=(-mx-1)) ? mx : v;
}

/*
 * New resampler code; replace the original somehwat blurry engine
 * with a fairly standard kernel resampling core.  This could be used
 * for full affine transformation but only implements scale/translate.
 * Mostly reuses the old blending macro code.
 *
 * Pixel convention:
 *
 *  1) Pixels are points, not areas or squares.
 *
 *  2) To maintain the usual edge and scaling conventions, pixels are
 *     set inward from the image edge, eg, the left edge of an image is
 *     at pixel location x=-.5, not x=0.  Although pixels are not
 *     squares, the usual way of stating this is 'the pixel is located
 *     at the center of its square'.
 *
 *  3) Because of 1 and 2, we must truncate and weight the kernel
 *     convolution at the edge of the input area.  Otherwise, all
 *     resampled areas would be bordered by a transparency halo. E.g.
 *     in the old engine, upsampling HDV to 1920x1080 results in the
 *     left and right edges being partially transparent and underlying
 *     layers shining through.
 *
 *   4) The contribution of fractional pixels at the edges of input
 *     ranges are weighted according to the fraction.  Note that the
 *     kernel weighting is adjusted, not the opacity.  This is one
 *     exception to 'pixels have no area'.
 *
 *  5) The opacity of fractional pixels at the edges of the output
 *     range is adjusted according to the fraction. This is the other
 *     exception to 'pixels have no area'.
 *
 * Fractional alpha blending has been modified across the board from:
 *    output_alpha = input_alpha > output_alpha ? input_alpha : output_alpha;
 *  to:
 *    output_alpha = output_alpha + ((max - output_alpha) * input_alpha) / max;
 */

#define TRANSFORM_SPP    (4096)    /* number of data pts per unit x in lookup table */
#define INDEX_FRACTION   (8)       /* bits of fraction past TRANSFORM_SPP on kernel
                                      index accumulation */
#define TRANSFORM_MIN    (.5 / TRANSFORM_SPP)

/* Sinc needed for Lanczos kernel */
static float sinc(const float x)
{
	float y = x * M_PI;

	if(fabsf(x) < TRANSFORM_MIN)
		return 1.0f;

	return sinf(y) / y;
}

/*
 * All resampling (except Nearest Neighbor) is performed via
 *   transformed 2D resampling kernels bult from 1D lookups.
 */
OverlayKernel::OverlayKernel(int interpolation_type)
{
	int i;
	this->type = interpolation_type;

	switch(interpolation_type)
	{
	case BILINEAR:
		width = 1.f;
		lookup = new float[(n = TRANSFORM_SPP) + 1];
		for (i = 0; i <= TRANSFORM_SPP; i++)
			lookup[i] = (float)(TRANSFORM_SPP - i) / TRANSFORM_SPP;
		break;

	/* Use a Catmull-Rom filter (not b-spline) */
	case BICUBIC:
		width = 2.;
		lookup = new float[(n = 2 * TRANSFORM_SPP) + 1];
		for(i = 0; i <= TRANSFORM_SPP; i++) {
			float x = i / (float)TRANSFORM_SPP;
			lookup[i] = 1.f - 2.5f * x * x + 1.5f * x * x * x;
		}
		for(; i <= 2 * TRANSFORM_SPP; i++) {
			float x = i / (float)TRANSFORM_SPP;
			lookup[i] = 2.f - 4.f * x  + 2.5f * x * x - .5f * x * x * x;
		}
		break;

	case LANCZOS:
		width = 3.;
		lookup = new float[(n = 3 * TRANSFORM_SPP) + 1];
		for (i = 0; i <= 3 * TRANSFORM_SPP; i++)
			lookup[i] = sinc((float)i / TRANSFORM_SPP) *
				sinc((float)i / TRANSFORM_SPP / 3.0f);
		break;

	default:
		width = 0.;
		lookup = 0;
		n = 0;
		break;
	}
}

OverlayKernel::~OverlayKernel()
{
	if(lookup) delete [] lookup;
}

OverlayFrame::OverlayFrame(int cpus)
{
	direct_engine = 0;
	nn_engine = 0;
	sample_engine = 0;
	temp_frame = 0;
	memset(kernel, 0, sizeof(kernel));
	this->cpus = cpus;
}

OverlayFrame::~OverlayFrame()
{
	if(temp_frame) delete temp_frame;

	if(direct_engine) delete direct_engine;
	if(nn_engine) delete nn_engine;
	if(sample_engine) delete sample_engine;

	if(kernel[NEAREST_NEIGHBOR]) delete kernel[NEAREST_NEIGHBOR];
	if(kernel[BILINEAR]) delete kernel[BILINEAR];
	if(kernel[BICUBIC]) delete kernel[BICUBIC];
	if(kernel[LANCZOS]) delete kernel[LANCZOS];
}

static float epsilon_snap(float f)
{
	return rintf(f * 1024) / 1024.;
}

int OverlayFrame::overlay(VFrame *output, VFrame *input,
	float in_x1, float in_y1, float in_x2, float in_y2,
	float out_x1, float out_y1, float out_x2, float out_y2,
	float alpha, int mode, int interpolation_type)
{
	in_x1 = epsilon_snap(in_x1);
	in_x2 = epsilon_snap(in_x2);
	in_y1 = epsilon_snap(in_y1);
	in_y2 = epsilon_snap(in_y2);
	out_x1 = epsilon_snap(out_x1);
	out_x2 = epsilon_snap(out_x2);
	out_y1 = epsilon_snap(out_y1);
	out_y2 = epsilon_snap(out_y2);

	if (isnan(in_x1) || isnan(in_x2) ||
		isnan(in_y1) || isnan(in_y2) ||
		isnan(out_x1) || isnan(out_x2) ||
		isnan(out_y1) || isnan(out_y2)) return 1;

	if(in_x1 < 0) in_x1 = 0;
	if(in_y1 < 0) in_y1 = 0;
	if(in_x2 > input->get_w()) in_x2 = input->get_w();
	if(in_y2 > input->get_h()) in_y2 = input->get_h();
	if(out_x1 < 0) out_x1 = 0;
	if(out_y1 < 0) out_y1 = 0;
	if(out_x2 > output->get_w()) out_x2 = output->get_w();
	if(out_y2 > output->get_h()) out_y2 = output->get_h();

	float xscale = (out_x2 - out_x1) / (in_x2 - in_x1);
	float yscale = (out_y2 - out_y1) / (in_y2 - in_y1);

	/* don't interpolate integer translations, or scale no-ops */
	if(xscale == 1. && yscale == 1. &&
		(int)in_x1 == in_x1 && (int)in_x2 == in_x2 &&
		(int)in_y1 == in_y1 && (int)in_y2 == in_y2 &&
		(int)out_x1 == out_x1 && (int)out_x2 == out_x2 &&
		(int)out_y1 == out_y1 && (int)out_y2 == out_y2) {
		if(!direct_engine) direct_engine = new DirectEngine(cpus);

		direct_engine->output = output;   direct_engine->input = input;
		direct_engine->in_x1 = in_x1;     direct_engine->in_y1 = in_y1;
		direct_engine->out_x1 = out_x1;   direct_engine->out_x2 = out_x2;
		direct_engine->out_y1 = out_y1;   direct_engine->out_y2 = out_y2;
		direct_engine->alpha = alpha;     direct_engine->mode = mode;
		direct_engine->process_packages();
	}
	else if(interpolation_type == NEAREST_NEIGHBOR) {
		if(!nn_engine) nn_engine = new NNEngine(cpus);
		nn_engine->output = output;       nn_engine->input = input;
		nn_engine->in_x1 = in_x1;         nn_engine->in_x2 = in_x2;
		nn_engine->in_y1 = in_y1;         nn_engine->in_y2 = in_y2;
		nn_engine->out_x1 = out_x1;       nn_engine->out_x2 = out_x2;
		nn_engine->out_y1 = out_y1;       nn_engine->out_y2 = out_y2;
		nn_engine->alpha = alpha;         nn_engine->mode = mode;
		nn_engine->process_packages();
	}
	else {
		int xtype = BILINEAR;
		int ytype = BILINEAR;

		switch(interpolation_type)
		{
		case CUBIC_CUBIC: // Bicubic enlargement and reduction
			xtype = ytype = BICUBIC;
			break;
		case CUBIC_LINEAR: // Bicubic enlargement and bilinear reduction
			xtype = xscale > 1. ? BICUBIC : BILINEAR;
			ytype = yscale > 1. ? BICUBIC : BILINEAR;
			break;
		case LINEAR_LINEAR: // Bilinear enlargement and bilinear reduction
			xtype = ytype = BILINEAR;
			break;
		case LANCZOS_LANCZOS: // Because we can
			xtype = ytype = LANCZOS;
			break;
		}

		if(xscale == 1. && (int)in_x1 == in_x1 && (int)in_x2 == in_x2 &&
				(int)out_x1 == out_x1 && (int)out_x2 == out_x2)
			xtype = DIRECT_COPY;

		if(yscale == 1. && (int)in_y1 == in_y1 && (int)in_y2 == in_y2 &&
				(int)out_y1 == out_y1 && (int)out_y2 == out_y2)
			ytype = DIRECT_COPY;

		if(!kernel[xtype])
			kernel[xtype] = new OverlayKernel(xtype);
		if(!kernel[ytype])
			kernel[ytype] = new OverlayKernel(ytype);

/*
 * horizontal and vertical are separately resampled.  First we
 * resample the input along X into a transposed, temporary frame,
 * then resample/transpose the temporary space along X into the
 * output.  Fractional pixels along the edge are handled in the X
 * direction of each step
 */
		// resampled dimension matches the transposed output space
		float temp_y1 = out_x1 - floor(out_x1);
		float temp_y2 = temp_y1 + (out_x2 - out_x1);
		int temp_h = ceil(temp_y2);

		// non-resampled dimension merely cropped
		float temp_x1 = in_y1 - floor(in_y1);
		float temp_x2 = temp_x1 + (in_y2 - in_y1);
		int temp_w = ceil(temp_x2);

		if( temp_frame &&
		   (temp_frame->get_color_model() != input->get_color_model() ||
                    temp_frame->get_w() != temp_w || temp_frame->get_h() != temp_h) ) {
			delete temp_frame;
			temp_frame = 0;
		}

		if(!temp_frame) {
			temp_frame = new VFrame(0, -1, temp_w, temp_h,
				input->get_color_model(), -1);
		}

		temp_frame->clear_frame();

		if(!sample_engine) sample_engine = new SampleEngine(cpus);

		sample_engine->output = temp_frame;
		sample_engine->input = input;
		sample_engine->kernel = kernel[xtype];
		sample_engine->col_out1 = 0;
		sample_engine->col_out2 = temp_w;
		sample_engine->row_in = floor(in_y1);

		sample_engine->in1 = in_x1;
		sample_engine->in2 = in_x2;
		sample_engine->out1 = temp_y1;
		sample_engine->out2 = temp_y2;
		sample_engine->alpha = 1.;
		sample_engine->mode = TRANSFER_REPLACE;
		sample_engine->process_packages();

		sample_engine->output = output;
		sample_engine->input = temp_frame;
		sample_engine->kernel = kernel[ytype];
		sample_engine->col_out1 = floor(out_x1);
		sample_engine->col_out2 = ceil(out_x2);
		sample_engine->row_in = 0;

		sample_engine->in1 = temp_x1;
		sample_engine->in2 = temp_x2;
		sample_engine->out1 = out_y1;
		sample_engine->out2 = out_y2;
		sample_engine->alpha = alpha;
		sample_engine->mode = mode;
		sample_engine->process_packages();
	}
	return 0;
}

// NORMAL	[Sa * Sa + Da * (1 - Sa), Sc * Sa + Dc * (1 - Sa)])
#define ALPHA_NORMAL(mx, Sa, Da) ((Sa * Sa + Da * (mx - Sa)) / mx)
#define COLOR_NORMAL(mx, Sc, Sa, Dc, Da) ((Sc * Sa + Dc * (mx - Sa)) / mx)
#define CHROMA_NORMAL COLOR_NORMAL

// ADDITION	[(Sa + Da), (Sc + Dc)]
#define ALPHA_ADDITION(mx, Sa, Da) (Sa + Da)
#define COLOR_ADDITION(mx, Sc, Sa, Dc, Da) (Sc + Dc)
#define CHROMA_ADDITION(mx, Sc, Sa, Dc, Da) (Sc + Dc)

// SUBTRACT	[(Sa - Da), (Sc - Dc)]
#define ALPHA_SUBTRACT(mx, Sa, Da) (Sa - Da)
#define COLOR_SUBTRACT(mx, Sc, Sa, Dc, Da) (Sc - Dc)
#define CHROMA_SUBTRACT(mx, Sc, Sa, Dc, Da) (Sc - Dc)

// MULTIPLY	[(Sa * Da), Sc * Dc]
#define ALPHA_MULTIPLY(mx, Sa, Da) ((Sa * Da) / mx)
#define COLOR_MULTIPLY(mx, Sc, Sa, Dc, Da) ((Sc * Dc) / mx)
#define CHROMA_MULTIPLY(mx, Sc, Sa, Dc, Da) ((Sc * Dc) / mx)

// DIVIDE	[(Sa / Da), (Sc / Dc)]
#define ALPHA_DIVIDE(mx, Sa, Da) (Da ? ((Sa * mx) / Da) : mx)
#define COLOR_DIVIDE(mx, Sc, Sa, Dc, Da) (Dc ? ((Sc * mx) / Dc) : mx)
#define CHROMA_DIVIDE(mx, Sc, Sa, Dc, Da) (Dc ? ((Sc * mx) / Dc) : mx)

// REPLACE	[Sa, Sc] (fade = 1)
#define ALPHA_REPLACE(mx, Sa, Da) Sa
#define COLOR_REPLACE(mx, Sc, Sa, Dc, Da) Sc
#define CHROMA_REPLACE COLOR_REPLACE

// MAX		[max(Sa, Da), MAX(Sc, Dc)]
#define ALPHA_MAX(mx, Sa, Da) (Sa > Da ? Sa : Da)
#define COLOR_MAX(mx, Sc, Sa, Dc, Da) (Sc > Dc ? Sc : Dc)
#define CHROMA_MAX(mx, Sc, Sa, Dc, Da) (Sc > Dc ? Sc : Dc)

// MIN		[min(Sa, Da), MIN(Sc, Dc)]
#define ALPHA_MIN(mx, Sa, Da) (Sa < Da ? Sa : Da)
#define COLOR_MIN(mx, Sc, Sa, Dc, Da) (Sc < Dc ? Sc : Dc)
#define CHROMA_MIN(mx, Sc, Sa, Dc, Da) (Sc < Dc ? Sc : Dc)

// AVERAGE	[(Sa + Da) * 0.5, (Sc + Dc) * 0.5]
#define ALPHA_AVERAGE(mx, Sa, Da) ((Sa + Da) / 2)
#define COLOR_AVERAGE(mx, Sc, Sa, Dc, Da) ((Sc + Dc) / 2)
#define CHROMA_AVERAGE COLOR_AVERAGE

// DARKEN  	[Sa + Da - Sa*Da, Sc*(1 - Da) + Dc*(1 - Sa) + min(Sc, Dc)]  
#define ALPHA_DARKEN(mx, Sa, Da) (Sa + Da - (Sa * Da) / mx)
#define COLOR_DARKEN(mx, Sc, Sa, Dc, Da) ((Sc * (mx - Da) + Dc * (mx - Sa)) / mx + (Sc < Dc ? Sc : Dc))
#define CHROMA_DARKEN COLOR_DARKEN

// LIGHTEN  	[Sa + Da - Sa*Da, Sc*(1 - Da) + Dc*(1 - Sa) + max(Sc, Dc)]  
#define ALPHA_LIGHTEN(mx, Sa, Da) (Sa + Da - Sa * Da / mx)
#define COLOR_LIGHTEN(mx, Sc, Sa, Dc, Da) ((Sc * (mx - Da) + Dc * (mx - Sa)) / mx + (Sc > Dc ? Sc : Dc))
#define CHROMA_LIGHTEN COLOR_LIGHTEN

// DST  	[Da, Dc]  
#define ALPHA_DST(mx, Sa, Da) Da
#define COLOR_DST(mx, Sc, Sa, Dc, Da) Dc
#define CHROMA_DST COLOR_DST

// DST_ATOP  	[Sa, Sc * (1 - Da) + Dc * Sa]
#define ALPHA_DST_ATOP(mx, Sa, Da) Sa
#define COLOR_DST_ATOP(mx, Sc, Sa, Dc, Da) ((Sc * (mx - Da) + Dc * Sa) / mx)
#define CHROMA_DST_ATOP COLOR_DST_ATOP

// DST_IN  	[Da * Sa, Dc * Sa]  
#define ALPHA_DST_IN(mx, Sa, Da) ((Da * Sa) / mx)
#define COLOR_DST_IN(mx, Sc, Sa, Dc, Da) ((Dc * Sa) / mx)
#define CHROMA_DST_IN COLOR_DST_IN

// DST_OUT  	[Da * (1 - Sa), Dc * (1 - Sa)]  
#define ALPHA_DST_OUT(mx, Sa, Da) (Da * (mx - Sa) / mx)
#define COLOR_DST_OUT(mx, Sc, Sa, Dc, Da) (Dc * (mx - Sa) / mx)
#define CHROMA_DST_OUT COLOR_DST_OUT

// DST_OVER  	[Sa * (1 - Da) + Da, Sc * (1 - Da) + Dc]  
#define ALPHA_DST_OVER(mx, Sa, Da) ((Sa * (mx - Da)) / mx + Da)
#define COLOR_DST_OVER(mx, Sc, Sa, Dc, Da) (Sc * (mx - Da)/ mx + Dc)
#define CHROMA_DST_OVER COLOR_DST_OVER

// SRC  		[Sa, Sc]  
#define ALPHA_SRC(mx, Sa, Da) Sa
#define COLOR_SRC(mx, Sc, Sa, Dc, Da) Sc
#define CHROMA_SRC COLOR_SRC

// SRC_ATOP  	[Da, Sc * Da + Dc * (1 - Sa)]  
#define ALPHA_SRC_ATOP(mx, Sa, Da) Da
#define COLOR_SRC_ATOP(mx, Sc, Sa, Dc, Da) ((Sc * Da + Dc * (mx - Sa)) / mx)
#define CHROMA_SRC_ATOP COLOR_SRC_ATOP

// SRC_IN  	[Sa * Da, Sc * Da]  
#define ALPHA_SRC_IN(mx, Sa, Da) ((Sa * Da) / mx)
#define COLOR_SRC_IN(mx, Sc, Sa, Dc, Da) (Sc * Da / mx)
#define CHROMA_SRC_IN COLOR_SRC_IN

// SRC_OUT  	[Sa * (1 - Da), Sc * (1 - Da)]  
#define ALPHA_SRC_OUT(mx, Sa, Da) (Sa * (mx - Da) / mx)
#define COLOR_SRC_OUT(mx, Sc, Sa, Dc, Da) (Sc * (mx - Da) / mx)
#define CHROMA_SRC_OUT COLOR_SRC_OUT

// SRC_OVER  	[Sa + Da * (1 - Sa), Sc + (1 - Sa) * Dc]  
#define ALPHA_SRC_OVER(mx, Sa, Da) (Sa + Da * (mx - Sa) / mx)
#define COLOR_SRC_OVER(mx, Sc, Sa, Dc, Da) (Sc + Dc * (mx - Sa) / mx)
#define CHROMA_SRC_OVER COLOR_SRC_OVER

// OR  	[Sa + Da - Sa * Da, Sc + Dc - Sc * Dc]   
#define ALPHA_OR(mx, Sa, Da) (Sa + Da - (Sa * Da) / mx)
#define COLOR_OR(mx, Sc, Sa, Dc, Da) (Sc + Dc - (Sc * Dc) / mx)
#define CHROMA_OR COLOR_OR

// XOR  	[Sa * (1 - Da) + Da * (1 - Sa), Sc * (1 - Da) + Dc * (1 - Sa)]   
#define ALPHA_XOR(mx, Sa, Da) ((Sa * (mx - Da) + Da * (mx - Sa)) / mx)
#define COLOR_XOR(mx, Sc, Sa, Dc, Da) ((Sc * (mx - Da) + Dc * (mx - Sa)) / mx)
#define CHROMA_XOR COLOR_XOR


#define ALPHA3_BLEND(FN, typ, inp, out, mx, ofs, rnd) \
  typ inp0 = (typ)inp[0], inp1 = (typ)inp[1] - ofs; \
  typ inp2 = (typ)inp[2] - ofs, inp3 = fade * mx + rnd; \
  typ out0 = (typ)out[0], out1 = (typ)out[1] - ofs; \
  typ out2 = (typ)out[2] - ofs, out3 = mx; \
  r = aclip(COLOR_##FN(mx, inp0, inp3, out0, out3), mx); \
  if( ofs ) { \
    g = cclip(CHROMA_##FN(mx, inp1, inp3, out1, out3), mx); \
    b = cclip(CHROMA_##FN(mx, inp2, inp3, out2, out3), mx); \
  } \
  else { \
    g = aclip(COLOR_##FN(mx, inp1, inp3, out1, out3), mx); \
    b = aclip(COLOR_##FN(mx, inp2, inp3, out2, out3), mx); \
  }

#define ALPHA4_BLEND(FN, typ, inp, out, mx, ofs, rnd) \
  typ inp0 = (typ)inp[0], inp1 = (typ)inp[1] - ofs; \
  typ inp2 = (typ)inp[2] - ofs, inp3 = (typ)inp[3] * fade + rnd; \
  typ out0 = (typ)out[0], out1 = (typ)out[1] - ofs; \
  typ out2 = (typ)out[2] - ofs, out3 = out[3]; \
  r = aclip(COLOR_##FN(mx, inp0, inp3, out0, out3), mx); \
  if( ofs ) { \
    g = cclip(CHROMA_##FN(mx, inp1, inp3, out1, out3), mx); \
    b = cclip(CHROMA_##FN(mx, inp2, inp3, out2, out3), mx); \
  } \
  else { \
    g = aclip(COLOR_##FN(mx, inp1, inp3, out1, out3), mx); \
    b = aclip(COLOR_##FN(mx, inp2, inp3, out2, out3), mx); \
  } \
  a = aclip(ALPHA_##FN(mx, inp3, out3), mx); \

#define ALPHA_STORE(out, ofs, mx) \
  out[0] = r; \
  out[1] = g + ofs; \
  out[2] = b + ofs

#define ALPHA3_STORE(out, ofs, mx) \
  if( trnsp ) { \
    r = (r * opcty + out0 * trnsp) / mx; \
    g = (g * opcty + out1 * trnsp) / mx; \
    b = (b * opcty + out2 * trnsp) / mx; \
  } \
  ALPHA_STORE(out, ofs, mx)

#define ALPHA4_STORE(out, ofs, mx) \
  if( trnsp ) { \
    r = (r * opcty + out0 * trnsp) / mx; \
    g = (g * opcty + out1 * trnsp) / mx; \
    b = (b * opcty + out2 * trnsp) / mx; \
    a = (a * opcty + out3 * trnsp) / mx; \
  } \
  ALPHA_STORE(out, ofs, mx); \
  out[3] = a

#define XBLEND(FN, temp_type, type, max, components, chroma_offset, round) { \
	temp_type opcty = alpha * max + round, trnsp = max - opcty; \
	type** output_rows = (type**)output->get_rows(); \
	type** input_rows = (type**)input->get_rows(); \
	ix *= components;  ox *= components; \
 \
	for(int i = pkg->out_row1; i < pkg->out_row2; i++) { \
		type* in_row = input_rows[i + iy] + ix; \
		type* output = output_rows[i] + ox; \
		for(int j = 0; j < ow; j++) { \
			if( components == 4 ) { \
				temp_type r, g, b, a; \
				ALPHA4_BLEND(FN, temp_type, in_row, output, max, chroma_offset, round); \
				ALPHA4_STORE(output, chroma_offset, max); \
			} \
			else { \
				temp_type r, g, b; \
				ALPHA3_BLEND(FN, temp_type, in_row, output, max, chroma_offset, round); \
				ALPHA3_STORE(output, chroma_offset, max); \
			} \
			in_row += components;  output += components; \
		} \
	} \
	break; \
}

#define XBLEND_ONLY(FN) { \
	switch(input->get_color_model()) { \
	case BC_RGB_FLOAT:	XBLEND(FN, float,   float,    1.f,    3, 0,      0.f); \
	case BC_RGBA_FLOAT:	XBLEND(FN, float,   float,    1.f,    4, 0,      0.f); \
	case BC_RGB888:		XBLEND(FN, int32_t, uint8_t,  0xff,   3, 0,      .5f); \
	case BC_YUV888:		XBLEND(FN, int32_t, uint8_t,  0xff,   3, 0x80,   .5f); \
	case BC_RGBA8888:	XBLEND(FN, int32_t, uint8_t,  0xff,   4, 0,      .5f); \
	case BC_YUVA8888:	XBLEND(FN, int32_t, uint8_t,  0xff,   4, 0x80,   .5f); \
	case BC_RGB161616:	XBLEND(FN, int64_t, uint16_t, 0xffff, 3, 0,      .5f); \
	case BC_YUV161616:	XBLEND(FN, int64_t, uint16_t, 0xffff, 3, 0x8000, .5f); \
	case BC_RGBA16161616:	XBLEND(FN, int64_t, uint16_t, 0xffff, 4, 0,      .5f); \
	case BC_YUVA16161616:	XBLEND(FN, int64_t, uint16_t, 0xffff, 4, 0x8000, .5f); \
	} \
	break; \
}

/* Direct translate / blend **********************************************/

DirectPackage::DirectPackage()
{
}

DirectUnit::DirectUnit(DirectEngine *server)
 : LoadClient(server)
{
	this->engine = server;
}

DirectUnit::~DirectUnit()
{
}

void DirectUnit::process_package(LoadPackage *package)
{
	DirectPackage *pkg = (DirectPackage*)package;

	VFrame *output = engine->output;
	VFrame *input = engine->input;
	int mode = engine->mode;
	float fade = engine->alpha;
	float alpha = mode == TRANSFER_REPLACE ? 1.f : engine->alpha;

	int ix = engine->in_x1;
	int ox = engine->out_x1;
	int ow = engine->out_x2 - ox;
	int iy = engine->in_y1 - engine->out_y1;

	switch( mode ) {
        case TRANSFER_NORMAL: 		XBLEND_ONLY(NORMAL);
        case TRANSFER_ADDITION:		XBLEND_ONLY(ADDITION);
        case TRANSFER_SUBTRACT:		XBLEND_ONLY(SUBTRACT);
        case TRANSFER_MULTIPLY:		XBLEND_ONLY(MULTIPLY);
        case TRANSFER_DIVIDE: 		XBLEND_ONLY(DIVIDE);
        case TRANSFER_REPLACE: 		XBLEND_ONLY(REPLACE);
        case TRANSFER_MAX: 		XBLEND_ONLY(MAX);
        case TRANSFER_MIN: 		XBLEND_ONLY(MIN);
	case TRANSFER_AVERAGE:		XBLEND_ONLY(AVERAGE);
	case TRANSFER_DARKEN:		XBLEND_ONLY(DARKEN);
	case TRANSFER_LIGHTEN:		XBLEND_ONLY(LIGHTEN);
	case TRANSFER_DST:		XBLEND_ONLY(DST);
	case TRANSFER_DST_ATOP:		XBLEND_ONLY(DST_ATOP);
	case TRANSFER_DST_IN:		XBLEND_ONLY(DST_IN);
	case TRANSFER_DST_OUT:		XBLEND_ONLY(DST_OUT);
	case TRANSFER_DST_OVER:		XBLEND_ONLY(DST_OVER);
	case TRANSFER_SRC:		XBLEND_ONLY(SRC);
	case TRANSFER_SRC_ATOP:		XBLEND_ONLY(SRC_ATOP);
	case TRANSFER_SRC_IN:		XBLEND_ONLY(SRC_IN);
	case TRANSFER_SRC_OUT:		XBLEND_ONLY(SRC_OUT);
	case TRANSFER_SRC_OVER:		XBLEND_ONLY(SRC_OVER);
	case TRANSFER_OR:		XBLEND_ONLY(OR);
	case TRANSFER_XOR:		XBLEND_ONLY(XOR);
	}
}

DirectEngine::DirectEngine(int cpus)
 : LoadServer(cpus, cpus)
{
}

DirectEngine::~DirectEngine()
{
}

void DirectEngine::init_packages()
{
	if(in_x1 < 0) { out_x1 -= in_x1; in_x1 = 0; }
	if(in_y1 < 0) { out_y1 -= in_y1; in_y1 = 0; }
	if(out_x1 < 0) { in_x1 -= out_x1; out_x1 = 0; }
	if(out_y1 < 0) { in_y1 -= out_y1; out_y1 = 0; }
	if(out_x2 > output->get_w()) out_x2 = output->get_w();
	if(out_y2 > output->get_h()) out_y2 = output->get_h();
	int out_w = out_x2 - out_x1;
	int out_h = out_y2 - out_y1;
	if( !out_w || !out_h ) return;

	int rows = out_h;
	int pkgs = get_total_packages();
	int row1 = out_y1, row2 = row1;
	for(int i = 0; i < pkgs; row1=row2 ) {
		DirectPackage *package = (DirectPackage*)get_package(i);
		row2 = ++i * rows / pkgs + out_y1;
		package->out_row1 = row1;
		package->out_row2 = row2;
	}
}

LoadClient* DirectEngine::new_client()
{
	return new DirectUnit(this);
}

LoadPackage* DirectEngine::new_package()
{
	return new DirectPackage;
}

/* Nearest Neighbor scale / translate / blend ********************/

#define XBLEND_3NN(FN, temp_type, type, max, components, chroma_offset, round) { \
	temp_type opcty = alpha * max + round, trnsp = max - opcty; \
	type** output_rows = (type**)output->get_rows(); \
	type** input_rows = (type**)input->get_rows(); \
	ox *= components; \
 \
	for(int i = pkg->out_row1; i < pkg->out_row2; i++) { \
		int *lx = engine->in_lookup_x; \
		type* in_row = input_rows[*ly++]; \
		type* output = output_rows[i] + ox; \
		for(int j = 0; j < ow; j++) { \
			in_row += *lx++; \
			if( components == 4 ) { \
				temp_type r, g, b, a; \
				ALPHA4_BLEND(FN, temp_type, in_row, output, max, chroma_offset, round); \
				ALPHA4_STORE(output, chroma_offset, max); \
			} \
			else { \
				temp_type r, g, b; \
				ALPHA3_BLEND(FN, temp_type, in_row, output, max, chroma_offset, round); \
				ALPHA3_STORE(output, chroma_offset, max); \
			} \
			output += components; \
		} \
	} \
	break; \
}

#define XBLEND_NN(FN) { \
	switch(input->get_color_model()) { \
	case BC_RGB_FLOAT:	XBLEND_3NN(FN, float,   float,    1.f,    3, 0,       0.f); \
	case BC_RGBA_FLOAT:	XBLEND_3NN(FN, float,   float,    1.f,    4, 0,       0.f); \
	case BC_RGB888:		XBLEND_3NN(FN, int32_t, uint8_t,  0xff,   3, 0,      .5f); \
	case BC_YUV888:		XBLEND_3NN(FN, int32_t, uint8_t,  0xff,   3, 0x80,   .5f); \
	case BC_RGBA8888:	XBLEND_3NN(FN, int32_t, uint8_t,  0xff,   4, 0,      .5f); \
	case BC_YUVA8888:	XBLEND_3NN(FN, int32_t, uint8_t,  0xff,   4, 0x80,   .5f); \
	case BC_RGB161616:	XBLEND_3NN(FN, int64_t, uint16_t, 0xffff, 3, 0,      .5f); \
	case BC_YUV161616:	XBLEND_3NN(FN, int64_t, uint16_t, 0xffff, 3, 0x8000, .5f); \
	case BC_RGBA16161616:	XBLEND_3NN(FN, int64_t, uint16_t, 0xffff, 4, 0,      .5f); \
	case BC_YUVA16161616:	XBLEND_3NN(FN, int64_t, uint16_t, 0xffff, 4, 0x8000, .5f); \
	} \
	break; \
}

NNPackage::NNPackage()
{
}

NNUnit::NNUnit(NNEngine *server)
 : LoadClient(server)
{
	this->engine = server;
}

NNUnit::~NNUnit()
{
}

void NNUnit::process_package(LoadPackage *package)
{
	NNPackage *pkg = (NNPackage*)package;
	VFrame *output = engine->output;
	VFrame *input = engine->input;
	int mode = engine->mode;
	float fade = engine->alpha;
	float alpha = mode == TRANSFER_REPLACE ? 1.f : engine->alpha;

	int ox = engine->out_x1i;
	int ow = engine->out_x2i - ox;
	int *ly = engine->in_lookup_y + pkg->out_row1;

	switch( mode ) {
        case TRANSFER_NORMAL: 		XBLEND_NN(NORMAL);
        case TRANSFER_ADDITION:		XBLEND_NN(ADDITION);
        case TRANSFER_SUBTRACT:		XBLEND_NN(SUBTRACT);
        case TRANSFER_MULTIPLY:		XBLEND_NN(MULTIPLY);
        case TRANSFER_DIVIDE: 		XBLEND_NN(DIVIDE);
        case TRANSFER_REPLACE: 		XBLEND_NN(REPLACE);
        case TRANSFER_MAX: 		XBLEND_NN(MAX);
        case TRANSFER_MIN: 		XBLEND_NN(MIN);
	case TRANSFER_AVERAGE:		XBLEND_NN(AVERAGE);
	case TRANSFER_DARKEN:		XBLEND_NN(DARKEN);
	case TRANSFER_LIGHTEN:		XBLEND_NN(LIGHTEN);
	case TRANSFER_DST:		XBLEND_NN(DST);
	case TRANSFER_DST_ATOP:		XBLEND_NN(DST_ATOP);
	case TRANSFER_DST_IN:		XBLEND_NN(DST_IN);
	case TRANSFER_DST_OUT:		XBLEND_NN(DST_OUT);
	case TRANSFER_DST_OVER:		XBLEND_NN(DST_OVER);
	case TRANSFER_SRC:		XBLEND_NN(SRC);
	case TRANSFER_SRC_ATOP:		XBLEND_NN(SRC_ATOP);
	case TRANSFER_SRC_IN:		XBLEND_NN(SRC_IN);
	case TRANSFER_SRC_OUT:		XBLEND_NN(SRC_OUT);
	case TRANSFER_SRC_OVER:		XBLEND_NN(SRC_OVER);
	case TRANSFER_OR:		XBLEND_NN(OR);
	case TRANSFER_XOR:		XBLEND_NN(XOR);
	}
}

NNEngine::NNEngine(int cpus)
 : LoadServer(cpus, cpus)
{
	in_lookup_x = 0;
	in_lookup_y = 0;
}

NNEngine::~NNEngine()
{
	if(in_lookup_x)
		delete[] in_lookup_x;
	if(in_lookup_y)
		delete[] in_lookup_y;
}

void NNEngine::init_packages()
{
	int in_w = input->get_w();
	int in_h = input->get_h();
	int out_w = output->get_w();
	int out_h = output->get_h();

	float in_subw = in_x2 - in_x1;
	float in_subh = in_y2 - in_y1;
	float out_subw = out_x2 - out_x1;
	float out_subh = out_y2 - out_y1;
	int first, last, count, i;
	int components = 3;

	out_x1i = rint(out_x1);
	out_x2i = rint(out_x2);
	if(out_x1i < 0) out_x1i = 0;
	if(out_x1i > out_w) out_x1i = out_w;
	if(out_x2i < 0) out_x2i = 0;
	if(out_x2i > out_w) out_x2i = out_w;
	int out_wi = out_x2i - out_x1i;
	if( !out_wi ) return;

	delete[] in_lookup_x;
	in_lookup_x = new int[out_wi];
	delete[] in_lookup_y;
	in_lookup_y = new int[out_h];

	switch(input->get_color_model()) {
	case BC_RGBA_FLOAT:
	case BC_RGBA8888:
	case BC_YUVA8888:
	case BC_RGBA16161616:
		components = 4;
		break;
	}

	first = count = 0;

	for(i = out_x1i; i < out_x2i; i++) {
		int in = (i - out_x1 + .5) * in_subw / out_subw + in_x1;
		if(in < in_x1)
			in = in_x1;
		if(in > in_x2)
			in = in_x2;

		if(in >= 0 && in < in_w && in >= in_x1 && i >= 0 && i < out_w) {
			if(count == 0) {
				first = i;
				in_lookup_x[0] = in * components;
			}
			else {
				in_lookup_x[count] = (in-last)*components;
			}
			last = in;
			count++;
		}
		else if(count)
			break;
	}
	out_x1i = first;
	out_x2i = first + count;
	first = count = 0;

	for(i = out_y1; i < out_y2; i++) {
		int in = (i - out_y1+.5) * in_subh / out_subh + in_y1;
		if(in < in_y1) in = in_y1;
		if(in > in_y2) in = in_y2;
		if(in >= 0 && in < in_h && i >= 0 && i < out_h) {
			if(count == 0) first = i;
			in_lookup_y[i] = in;
			count++;
		}
		else if(count)
			break;
	}
	out_y1 = first;
	out_y2 = first + count;

	int rows = count;
	int pkgs = get_total_packages();
	int row1 = out_y1, row2 = row1;
	for(int i = 0; i < pkgs; row1=row2 ) {
		NNPackage *package = (NNPackage*)get_package(i);
		row2 = ++i * rows / pkgs + out_y1;
		package->out_row1 = row1;
		package->out_row2 = row2;
	}
}

LoadClient* NNEngine::new_client()
{
	return new NNUnit(this);
}

LoadPackage* NNEngine::new_package()
{
	return new NNPackage;
}

/* Fully resampled scale / translate / blend ******************************/
/* resample into a temporary row vector, then blend */

#define XSAMPLE(FN, temp_type, type, max, components, chroma_offset, round) { \
	float temp[oh*components]; \
	temp_type opcty = alpha * max + round, trnsp = max - opcty; \
	type **output_rows = (type**)voutput->get_rows() + o1i; \
	type **input_rows = (type**)vinput->get_rows(); \
 \
	for(int i = pkg->out_col1; i < pkg->out_col2; i++) { \
		type *input = input_rows[i - engine->col_out1 + engine->row_in]; \
		float *tempp = temp; \
		if( !k ) { /* direct copy case */ \
			type *ip = input + i1i * components; \
			for(int j = 0; j < oh; j++) { \
				*tempp++ = *ip++; \
				*tempp++ = *ip++ - chroma_offset; \
				*tempp++ = *ip++ - chroma_offset; \
				if( components == 4 ) *tempp++ = *ip++; \
			} \
		} \
		else { /* resample */ \
			for(int j = 0; j < oh; j++) { \
				float racc=0.f, gacc=0.f, bacc=0.f, aacc=0.f; \
				int ki = lookup_sk[j], x = lookup_sx0[j]; \
				type *ip = input + x * components; \
				float wacc = 0, awacc = 0; \
				while(x++ < lookup_sx1[j]) { \
					float kv = k[abs(ki >> INDEX_FRACTION)]; \
					/* handle fractional pixels on edges of input */ \
					if(x == i1i) kv *= i1f; \
					if(x + 1 == i2i) kv *= i2f; \
					if( components == 4 ) { awacc += kv;  kv *= ip[3]; } \
					wacc += kv; \
					racc += kv * *ip++; \
					gacc += kv * (*ip++ - chroma_offset); \
					bacc += kv * (*ip++ - chroma_offset); \
					if( components == 4 ) { aacc += kv;  ++ip; } \
					ki += kd; \
				} \
				if(wacc > 0.) wacc = 1. / wacc; \
				*tempp++ = racc * wacc; \
				*tempp++ = gacc * wacc; \
				*tempp++ = bacc * wacc; \
				if( components == 4 ) { \
					if(awacc > 0.) awacc = 1. / awacc; \
				 	*tempp++ = aacc * awacc; \
				} \
			} \
		} \
 \
		/* handle fractional pixels on edges of output */ \
		temp[0] *= o1f;   temp[1] *= o1f;   temp[2] *= o1f; \
		if( components == 4 ) temp[3] *= o1f; \
		tempp = temp + (oh-1)*components; \
		tempp[0] *= o2f;  tempp[1] *= o2f;  tempp[2] *= o2f; \
		if( components == 4 ) tempp[3] *= o2f; \
		tempp = temp; \
		/* blend output */ \
		for(int j = 0; j < oh; j++) { \
			type *output = output_rows[j] + i * components; \
			if( components == 4 ) { \
				temp_type r, g, b, a; \
				ALPHA4_BLEND(FN, temp_type, tempp, output, max, 0, round); \
				ALPHA4_STORE(output, chroma_offset, max); \
			} \
			else { \
				temp_type r, g, b; \
				ALPHA3_BLEND(FN, temp_type, tempp, output, max, 0, round); \
				ALPHA3_STORE(output, chroma_offset, max); \
			} \
			tempp += components; \
		} \
	} \
	break; \
}

#define XBLEND_SAMPLE(FN) { \
        switch(vinput->get_color_model()) { \
        case BC_RGB_FLOAT:      XSAMPLE(FN, float,   float,    1.f,    3, 0.f,    0.f); \
        case BC_RGBA_FLOAT:     XSAMPLE(FN, float,   float,    1.f,    4, 0.f,    0.f); \
        case BC_RGB888:         XSAMPLE(FN, int32_t, uint8_t,  0xff,   3, 0,      .5f); \
        case BC_YUV888:         XSAMPLE(FN, int32_t, uint8_t,  0xff,   3, 0x80,   .5f); \
        case BC_RGBA8888:       XSAMPLE(FN, int32_t, uint8_t,  0xff,   4, 0,      .5f); \
        case BC_YUVA8888:       XSAMPLE(FN, int32_t, uint8_t,  0xff,   4, 0x80,   .5f); \
        case BC_RGB161616:      XSAMPLE(FN, int64_t, uint16_t, 0xffff, 3, 0,      .5f); \
        case BC_YUV161616:      XSAMPLE(FN, int64_t, uint16_t, 0xffff, 3, 0x8000, .5f); \
        case BC_RGBA16161616:   XSAMPLE(FN, int64_t, uint16_t, 0xffff, 4, 0,      .5f); \
        case BC_YUVA16161616:   XSAMPLE(FN, int64_t, uint16_t, 0xffff, 4, 0x8000, .5f); \
        } \
        break; \
}


SamplePackage::SamplePackage()
{
}

SampleUnit::SampleUnit(SampleEngine *server)
 : LoadClient(server)
{
	this->engine = server;
}

SampleUnit::~SampleUnit()
{
}

void SampleUnit::process_package(LoadPackage *package)
{
	SamplePackage *pkg = (SamplePackage*)package;

	float i1  = engine->in1;
	float i2  = engine->in2;
	float o1  = engine->out1;
	float o2  = engine->out2;

	if(i2 - i1 <= 0 || o2 - o1 <= 0)
		return;

	VFrame *voutput = engine->output;
	VFrame *vinput = engine->input;
	int mode = engine->mode;
	float fade = engine->alpha;
	float alpha = mode == TRANSFER_REPLACE ? 1.f : engine->alpha;

	int   iw  = vinput->get_w();
	int   i1i = floor(i1);
	int   i2i = ceil(i2);
	float i1f = 1.f - i1 + i1i;
	float i2f = 1.f - i2i + i2;

	int   o1i = floor(o1);
	int   o2i = ceil(o2);
	float o1f = 1.f - o1 + o1i;
	float o2f = 1.f - o2i + o2;
	int   oh  = o2i - o1i;

	float *k  = engine->kernel->lookup;
	float kw  = engine->kernel->width;
	int   kn  = engine->kernel->n;
	int   kd = engine->kd;

	int *lookup_sx0 = engine->lookup_sx0;
	int *lookup_sx1 = engine->lookup_sx1;
	int *lookup_sk = engine->lookup_sk;
	float *lookup_wacc = engine->lookup_wacc;

	switch( mode ) {
        case TRANSFER_NORMAL: 		XBLEND_SAMPLE(NORMAL);
        case TRANSFER_ADDITION:		XBLEND_SAMPLE(ADDITION);
        case TRANSFER_SUBTRACT:		XBLEND_SAMPLE(SUBTRACT);
        case TRANSFER_MULTIPLY:		XBLEND_SAMPLE(MULTIPLY);
        case TRANSFER_DIVIDE: 		XBLEND_SAMPLE(DIVIDE);
        case TRANSFER_REPLACE: 		XBLEND_SAMPLE(REPLACE);
        case TRANSFER_MAX: 		XBLEND_SAMPLE(MAX);
        case TRANSFER_MIN: 		XBLEND_SAMPLE(MIN);
	case TRANSFER_AVERAGE:		XBLEND_SAMPLE(AVERAGE);
	case TRANSFER_DARKEN:		XBLEND_SAMPLE(DARKEN);
	case TRANSFER_LIGHTEN:		XBLEND_SAMPLE(LIGHTEN);
	case TRANSFER_DST:		XBLEND_SAMPLE(DST);
	case TRANSFER_DST_ATOP:		XBLEND_SAMPLE(DST_ATOP);
	case TRANSFER_DST_IN:		XBLEND_SAMPLE(DST_IN);
	case TRANSFER_DST_OUT:		XBLEND_SAMPLE(DST_OUT);
	case TRANSFER_DST_OVER:		XBLEND_SAMPLE(DST_OVER);
	case TRANSFER_SRC:		XBLEND_SAMPLE(SRC);
	case TRANSFER_SRC_ATOP:		XBLEND_SAMPLE(SRC_ATOP);
	case TRANSFER_SRC_IN:		XBLEND_SAMPLE(SRC_IN);
	case TRANSFER_SRC_OUT:		XBLEND_SAMPLE(SRC_OUT);
	case TRANSFER_SRC_OVER:		XBLEND_SAMPLE(SRC_OVER);
	case TRANSFER_OR:		XBLEND_SAMPLE(OR);
	case TRANSFER_XOR:		XBLEND_SAMPLE(XOR);
	}
}


SampleEngine::SampleEngine(int cpus)
 : LoadServer(cpus, cpus)
{
	lookup_sx0 = 0;
	lookup_sx1 = 0;
	lookup_sk = 0;
	lookup_wacc = 0;
	kd = 0;
}

SampleEngine::~SampleEngine()
{
	if(lookup_sx0) delete [] lookup_sx0;
	if(lookup_sx1) delete [] lookup_sx1;
	if(lookup_sk) delete [] lookup_sk;
	if(lookup_wacc) delete [] lookup_wacc;
}

/*
 * unlike the Direct and NN engines, the Sample engine works across
 * output columns (it makes for more economical memory addressing
 * during convolution)
 */
void SampleEngine::init_packages()
{
	int   iw  = input->get_w();
	int   i1i = floor(in1);
	int   i2i = ceil(in2);
	float i1f = 1.f - in1 + i1i;
	float i2f = 1.f - i2i + in2;

	int   oy  = floor(out1);
	float oyf = out1 - oy;
	int   oh  = ceil(out2) - oy;

	float *k  = kernel->lookup;
	float kw  = kernel->width;
	int   kn  = kernel->n;

	if(in2 - in1 <= 0 || out2 - out1 <= 0)
		return;

	/* determine kernel spatial coverage */
	float scale = (out2 - out1) / (in2 - in1);
	float iscale = (in2 - in1) / (out2 - out1);
	float coverage = fabs(1.f / scale);
	float bound = (coverage < 1.f ? kw : kw * coverage) - (.5f / TRANSFORM_SPP);
	float coeff = (coverage < 1.f ? 1.f : scale) * TRANSFORM_SPP;

	delete [] lookup_sx0;
	delete [] lookup_sx1;
	delete [] lookup_sk;
	delete [] lookup_wacc;

	lookup_sx0 = new int[oh];
	lookup_sx1 = new int[oh];
	lookup_sk = new int[oh];
	lookup_wacc = new float[oh];

	kd = (double)coeff * (1 << INDEX_FRACTION) + .5;

	/* precompute kernel values and weight sums */
	for(int i = 0; i < oh; i++) {
		/* map destination back to source */
		double sx = (i - oyf + .5) * iscale + in1 - .5;

		/*
		 * clip iteration to source area but not source plane. Points
		 * outside the source plane count as transparent. Points outside
		 * the source area don't count at all.  The actual convolution
		 * later will be clipped to both, but we need to compute
		 * weights.
		 */
		int sx0 = MAX((int)floor(sx - bound) + 1, i1i);
		int sx1 = MIN((int)ceil(sx + bound), i2i);
		int ki = (double)(sx0 - sx) * coeff * (1 << INDEX_FRACTION)
				+ (1 << (INDEX_FRACTION - 1)) + .5;
		float wacc=0.;

		lookup_sx0[i] = -1;
		lookup_sx1[i] = -1;

		for(int j= sx0; j < sx1; j++) {
			int kv = (ki >> INDEX_FRACTION);
			if(kv > kn) break;
			if(kv >= -kn) {
				/*
				 * the contribution of the first and last input pixel (if
				 * fractional) are linearly weighted by the fraction
				 */
				if(j == i1i)
					wacc += k[abs(kv)] * i1f;
				else if(j + 1 == i2i)
					wacc += k[abs(kv)] * i2f;
				else
					wacc += k[abs(kv)];

				/* this is where we clip the kernel convolution to the source plane */
				if(j >= 0 && j < iw) {
					if(lookup_sx0[i] == -1) {
						lookup_sx0[i] = j;
						lookup_sk[i] = ki;
					}
					lookup_sx1[i] = j + 1;
				}
			}
			ki += kd;
		}
		lookup_wacc[i] = wacc > 0. ? 1. / wacc : 0.;
	}

	int cols = col_out2 - col_out1;
	int pkgs = get_total_packages();
	int col1 = col_out1, col2 = col1;
	for(int i = 0; i < pkgs; col1=col2 ) {
		SamplePackage *package = (SamplePackage*)get_package(i);
		col2 = ++i * cols / pkgs + col_out1;
		package->out_col1 = col1;
		package->out_col2 = col2;
	}
}

LoadClient* SampleEngine::new_client()
{
	return new SampleUnit(this);
}

LoadPackage* SampleEngine::new_package()
{
	return new SamplePackage;
}
