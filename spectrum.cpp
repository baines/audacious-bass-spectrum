/*
* Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

/* Modifications by Alex Baines <alex@abaines.me.uk> */

extern "C" {

#include <audacious/plugin.h>
#include <audacious/misc.h>
#include <gtk/gtk.h>
#include <complex.h>
#include <fftw3.h>

}

#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

static const size_t FFT_SIZE = 40000;
static const size_t FFT_USED = 256;
static const size_t PCM_SAMPLES = 512;
static const size_t SAMPLE_CHUNKS = 2;
static const size_t REQUIRED_SAMPLES = PCM_SAMPLES * SAMPLE_CHUNKS;
static const size_t SKIP_BANDS = 8;
static const size_t MAX_BANDS = 256 - SKIP_BANDS;
static const size_t MAX_PIXEL_HEIGHT = 32;

static GtkWidget * spect_widget = NULL;
static gint width, height, bands;
static double bars[MAX_BANDS + 1] = { 0.0 };

using namespace std;

vector<double> samples;
fftw_complex *fft_out = NULL, *fft_twids = NULL;
double* fft_out1 = NULL;
fftw_plan fft_p;
fftw_r2r_kind fft_kind = FFTW_R2HC;

static void setup_fft(int N, int K, double* in){
  int i, j;
  const double TWOPI = 6.2831853071795864769252867665590057683943388;
  fft_out1 = (double*) fftw_malloc(N*sizeof(double));
  fft_out = (fftw_complex*) fftw_malloc(K*sizeof(fftw_complex));
  fft_twids = (fftw_complex*) fftw_malloc((K-1)*(N/K-1)*sizeof(fftw_complex));

  /* plan N/K FFTs of size K */
  fft_p = fftw_plan_many_r2r(1, &K, N/K,
                            in, NULL, N/K, 1, 
                            fft_out1, NULL, 1, K,
			    &fft_kind, FFTW_ESTIMATE | FFTW_PRESERVE_INPUT);

  /* precompute twiddle factors (since we usually want more than one FFT) */
  for (j = 1; j < N/K; ++j)
    for (i = 1; i < K; ++i)
      fft_twids[(j-1)*(K-1) + (i-1)] = cexp((I * FFTW_FORWARD * TWOPI/N) * (i*j));
}

static void do_fft(int N, int K, double* in){
  int i, j;
  fftw_execute(fft_p);

  /* set K elements of out[] to first K outputs of full size-N DFT: */
  fft_out[0] = fft_out1[0];
  for (i = 1; i+i < K; ++i) {
    double o1 = fft_out1[i], o2 = fft_out1[K-i];
    fft_out[i] = o1 + I*o2;
    fft_out[K-i] = o1 - I*o2;
  }
  if (i+i == K) /* Nyquist element */
    fft_out[i] = fft_out1[i];
  for (j = 1; j < N/K; ++j) {
    fft_out[0] += fft_out1[j*K];
    for (i = 1; i+i < K; ++i) {
      double o1 = fft_out1[i + j*K], o2 = fft_out1[K-i + j*K];
      fft_out[i] += (o1 + I*o2) * fft_twids[(j-1)*(K-1) + (i-1)];
      fft_out[K-i] += (o1 - I*o2) * fft_twids[(j-1)*(K-1) + (K-i-1)];
    }
    if (i+i == K) /* Nyquist element */
      fft_out[i] += fft_out1[i + j*K] * fft_twids[(j-1)*(K-1) + (i-1)];
  }
}

static double volume = 0.0;

static void render_cb (const float* pcm){

	g_return_if_fail (spect_widget);

	copy(pcm, pcm + PCM_SAMPLES, back_inserter(samples));
	
	double new_volume = 0.0;
	for(int i = 0; i < PCM_SAMPLES; ++i){
		new_volume += abs(pcm[i]);
	}

	new_volume = min(1.0, max(0.05, new_volume / (double)(PCM_SAMPLES/2.5)));
	volume = (volume * 0.8) + (new_volume * 0.2);
	
	for(int i = 0; i < PCM_SAMPLES; ++i){
		samples[i] /= volume;
	}
	
	if(samples.size() >= REQUIRED_SAMPLES){
		for(int i = 0; i < (FFT_SIZE - REQUIRED_SAMPLES); ++i){
			samples.push_back(0.0);
		}
		
		if(!fft_out){
			setup_fft(FFT_SIZE, FFT_USED, samples.data());
		}
		
		do_fft(FFT_SIZE, FFT_USED, samples.data());
				
		samples.erase(samples.begin() + REQUIRED_SAMPLES, samples.end());
		samples.erase(samples.begin(), samples.begin() + PCM_SAMPLES);
		
		int x = SKIP_BANDS;
		double m = 0.;
		
		for(int i = 0; i < bands; ++i){
			double v = cabs(fft_out[x++]);
			v = v / 64.;
				
			bars[i] = max(0., pow(max(0., v), 2.5 + (1. - volume)) - 0.2) * 0.25
			        + (bars[i] * 0.75);
			
			m = max(m, bars[i]);
		}
		
		if(m > (double)MAX_PIXEL_HEIGHT){
			double sc = (double)MAX_PIXEL_HEIGHT / m;
			for(int i = 0; i < bands; i++){
				bars[i] *= sc;
			}
		}
	}
	
	gtk_widget_queue_draw (spect_widget);
}

static void draw_background (GtkWidget * area, cairo_t * cr)
{
	GtkAllocation alloc;
	gtk_widget_get_allocation (area, & alloc);

	cairo_set_source_rgb (cr, 0.106f, 0.094f, 0.145f);
	cairo_rectangle(cr, 0, 0, alloc.width, alloc.height);
	cairo_fill (cr);
}

static void draw_visualizer (GtkWidget *widget, cairo_t *cr)
{
	for (gint i = 0; i <= bands; i++)
	{
		gint x = ((width / bands) * i) + 2;
		cairo_set_source_rgb (cr, 1.0f, 1.0f, 1.0f);
		cairo_rectangle (cr, x + 1, height - (bars[i] * 6.), (width / bands) - 4, bars[i] * 6.);
		cairo_fill (cr);
	}
}

static gboolean configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
	width = event->width;
	height = event->height;
	gtk_widget_queue_draw(widget);

	bands = width / 8;
	bands = CLAMP(bands, 12, MAX_BANDS);

	return TRUE;
}

#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean draw_event (GtkWidget * widget, cairo_t * cr, GtkWidget * area)
{
#else
static gboolean expose_event (GtkWidget * widget, GdkEventExpose * event, GtkWidget * area)
{
	cairo_t * cr = gdk_cairo_create (gtk_widget_get_window (widget));
#endif

	draw_background (widget, cr);
	draw_visualizer (widget, cr);

#if ! GTK_CHECK_VERSION (3, 0, 0)
	cairo_destroy (cr);
#endif

	return TRUE;
}

static gboolean destroy_event (void)
{
	aud_vis_func_remove ((VisFunc) render_cb);
	spect_widget = NULL;
	return TRUE;
}

extern "C" gpointer get_aud_widget(void)
{
	GtkWidget *area = gtk_drawing_area_new();
	spect_widget = area;

#if GTK_CHECK_VERSION (3, 0, 0)
	g_signal_connect(area, "draw", (GCallback) draw_event, NULL);
#else
	g_signal_connect(area, "expose-event", (GCallback) expose_event, NULL);
#endif
	g_signal_connect(area, "configure-event", (GCallback) configure_event, NULL);
	g_signal_connect(area, "destroy", (GCallback) destroy_event, NULL);

	aud_vis_func_add (AUD_VIS_TYPE_MONO_PCM, (VisFunc) &render_cb);
	
	return area;
}

