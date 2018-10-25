#ifndef SPECTRUM_H_
#define SPECTRUM_H_
#include <gtk/gtk.h>

gpointer get_aud_widget(void);
void render_cb (const float* pcm);

#endif
