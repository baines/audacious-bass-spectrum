#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#include "spectrum.h"

static constexpr PluginInfo _info = {
	"Audacious Bass Spectrum"
};

struct AudaciousBassSpectrum : VisPlugin {

	AudaciousBassSpectrum() : VisPlugin(_info, Visualizer::MonoPCM) {}

	void* get_gtk_widget() override {
		return get_aud_widget();
	}

	void render_mono_pcm(const float* pcm) override {
		render_cb(pcm);
	}

	void clear() override {

	}
};

extern "C" {
	AudaciousBassSpectrum aud_plugin_instance;
}
