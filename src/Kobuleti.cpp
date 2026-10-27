// Kobuleti: VCV Rack host for the open source Batumi firmware (MIT, Matthias Puech).
// The firmware's Lfo, Processor and lookup tables run unchanged at their native
// 16384 Hz rate; this file replaces the STM32 drivers with Rack's ports and params.
#include <rack.hpp>
#include <cstring>
#include "Controls4ms.hpp"
#include "Stencil.hpp"
#include "processor.h"

using namespace rack;

Plugin* pluginInstance;

static const float FIRMWARE_RATE = SAMPLE_RATE;
static const float FULL_SCALE_V = 5.f;  // ADC and DAC full scale (+/-5 V)

static int16_t toAdc(float v) {
	return (int16_t) clamp(v / FULL_SCALE_V * 32767.f, -32768.f, 32767.f);
}
static float fromDac(int16_t s) {
	// The PWM DAC driver inverts (32768 - value), the analog output stage does not
	return -s / 32768.f * FULL_SCALE_V;
}

struct Kobuleti : Module {
	enum ParamId {
		ENUMS(FREQ_PARAM, 4),
		SELECT_PARAM,
		SYNC_PARAM,
		WAV1_PARAM,
		WAV2_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(CV_INPUT, 4),
		ENUMS(RESET_INPUT, 4),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(SINE_OUTPUT, 4),
		ENUMS(ASGN_OUTPUT, 4),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(MODE_LIGHT, 4),
		ENUMS(CHANNEL_LIGHT, 4 * 2),
		LIGHTS_LEN
	};

	batumi::Ui ui;
	batumi::Adc adc;
	batumi::Dac dac;
	batumi::Processor processor;
	float firmwarePhase = 0.f;
	float msPhase = 0.f;
	bool initialized = false;
	int theme = 0;  // 0 = Bone (light), 1 = Slate (dark)

	Kobuleti() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < 4; i++) {
			configParam(FREQ_PARAM + i, 0.f, 1.f, 0.5f, string::f("Channel %d frequency / level / phase / division", i + 1));
			configInput(CV_INPUT + i, string::f("Channel %d CV", i + 1));
			configInput(RESET_INPUT + i, string::f("Channel %d reset / sync", i + 1));
			configOutput(SINE_OUTPUT + i, string::f("Channel %d sine", i + 1));
			configOutput(ASGN_OUTPUT + i, string::f("Channel %d assignable wave", i + 1));
		}
		configButton(SELECT_PARAM, "Mode select (short: next mode, hold: fine tune)");
		configSwitch(SYNC_PARAM, 0.f, 1.f, 0.f, "Reset input", {"Reset", "Sync"});
		configSwitch(WAV1_PARAM, 0.f, 1.f, 0.f, "Wave switch 1", {"Off", "On"});
		configSwitch(WAV2_PARAM, 0.f, 1.f, 0.f, "Wave switch 2", {"Off", "On"});
		// The firmware relies on zeroed static memory (it runs on a microcontroller), but a Rack
		// module lives on the heap with indeterminate contents. For example Lfo::Init() never sets
		// bl_step_counter_ or reset_subsample_, which then index the waveform tables out of range.
		std::memset((void*) &ui, 0, sizeof(ui));
		std::memset((void*) &adc, 0, sizeof(adc));
		std::memset((void*) &dac, 0, sizeof(dac));
		std::memset((void*) &processor, 0, sizeof(processor));
		processor.Init(&ui, &adc, &dac);
	}

	batumi::PanelState panel() {
		batumi::PanelState p;
		for (int i = 0; i < 4; i++)
			p.pot[i] = (uint16_t) std::round(clamp(params[FREQ_PARAM + i].getValue(), 0.f, 1.f) * 65535.f);
		p.sync = params[SYNC_PARAM].getValue() > 0.5f;
		p.wav1 = params[WAV1_PARAM].getValue() > 0.5f;
		p.wav2 = params[WAV2_PARAM].getValue() > 0.5f;
		p.select = params[SELECT_PARAM].getValue() > 0.5f;
		return p;
	}

	void onReset() override {
		ui.feat_mode_ = batumi::FEAT_MODE_FREE;
		for (int i = 0; i < 4; i++)
			ui.pot_fine_value_[i] = 1 << 15;
		initialized = false;
	}

	void process(const ProcessArgs& args) override {
		if (!initialized) {
			ui.Init(panel());
			processor.Init(&ui, &adc, &dac);
			initialized = true;
		}
		// UI runs from the 1 kHz SysTick
		msPhase += args.sampleTime * 1000.f;
		while (msPhase >= 1.f) {
			msPhase -= 1.f;
			ui.Poll(panel());
		}
		// Processing runs from the 16384 Hz timer interrupt
		firmwarePhase += args.sampleTime * FIRMWARE_RATE;
		while (firmwarePhase >= 1.f) {
			firmwarePhase -= 1.f;
			for (int i = 0; i < 4; i++) {
				adc.cv_[i] = toAdc(inputs[CV_INPUT + i].getVoltage());
				adc.reset_[i] = toAdc(inputs[RESET_INPUT + i].getVoltage());
			}
			processor.Process();
		}
		for (int i = 0; i < 4; i++) {
			float sine = fromDac(dac.sine_[i]);
			outputs[SINE_OUTPUT + i].setVoltage(sine);
			outputs[ASGN_OUTPUT + i].setVoltage(fromDac(dac.asgn_[i]));
			lights[CHANNEL_LIGHT + 2 * i + 0].setBrightnessSmooth(std::max(0.f, sine / FULL_SCALE_V), args.sampleTime);
			lights[CHANNEL_LIGHT + 2 * i + 1].setBrightnessSmooth(std::max(0.f, -sine / FULL_SCALE_V), args.sampleTime);
		}
		for (int i = 0; i < 4; i++)
			lights[MODE_LIGHT + i].setBrightness(ui.led(i) ? 1.f : 0.f);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "featMode", json_integer(ui.feat_mode_));
		json_t* fine = json_array();
		for (int i = 0; i < 4; i++)
			json_array_append_new(fine, json_integer(ui.pot_fine_value_[i]));
		json_object_set_new(root, "fine", fine);
		json_object_set_new(root, "theme", json_integer(theme));
		return root;
	}

	void dataFromJson(json_t* root) override {
		if (json_t* t = json_object_get(root, "theme"))
			theme = clamp((int) json_integer_value(t), 0, 1);
		json_t* m = json_object_get(root, "featMode");
		if (m)
			ui.feat_mode_ = (batumi::FeatureMode) clamp((int) json_integer_value(m), 0, 3);
		json_t* fine = json_object_get(root, "fine");
		for (int i = 0; fine && i < 4 && i < (int) json_array_size(fine); i++)
			ui.pot_fine_value_[i] = json_integer_value(json_array_get(fine, i));
	}
};

// ---------------------------------------------------------------- themes
// betobullet stencil (docs/art/STYLE.md): theme 0 = black panel, 1 = white panel
using bb::THEME_NAMES;
static const int NUM_THEMES = 2;
// LED colours: positive half white, negative half grey (the panel stays black and white)
static const NVGcolor LED_POSITIVE = nvgRGB(0xF4, 0xF4, 0xF2), LED_NEGATIVE = nvgRGB(0x8A, 0x8A, 0x88);

namespace fms {
const Colors& colors(const engine::Module* m) {
	static const Colors black = bb::controlColors(0), white = bb::controlColors(1);
	int t = m ? static_cast<const Kobuleti*>(m)->theme : 0;
	return t == 1 ? white : black;
}
}  // namespace fms

static std::shared_ptr<window::Font> pluginFont(const char* file) {
	std::shared_ptr<window::Font> f = APP->window->loadFont(asset::plugin(pluginInstance, std::string("res/fonts/") + file));
	if (!f)
		f = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
	return f;
}

// ---------------------------------------------------------------- widgets
/** Panel background, stencil frames and every label. Drawn in code so the theme can change. */
struct PanelArt : Widget {
	Kobuleti* module = nullptr;

	void draw(const DrawArgs& args) override {
		bb::Ink k = bb::ink(module ? module->theme : 0);
		bb::Ink inv = {k.fg, k.bg};
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(vg, k.bg);
		nvgFill(vg);

		// frames: select + modes, the two input rows; the outputs on a solid plate; the switches
		bb::bullets(vg, 4.0f, 46.8f, 16.4f, 2.4f, 10, k);
		bb::notchedFrame(vg, Rect(Vec(2.4f, 18.2f), Vec(46.0f, 12.4f)), k, 2.0f, 1.2f);
		bb::frame(vg, Rect(Vec(2.4f, 52.2f), Vec(46.0f, 26.8f)), k);
		const float X[4] = {8.2f, 19.6f, 31.0f, 42.4f};
		for (int i = 0; i < 3; i++) {
			bb::link(vg, X[i], X[i + 1], 58.2f, k, 3.6f);
			bb::link(vg, X[i], X[i + 1], 70.4f, k, 3.6f);
		}
		bb::chamferPath(vg, Rect(Vec(2.4f, 87.0f), Vec(46.0f, 26.4f)), 2.0f);
		nvgFillColor(vg, k.fg);
		nvgFill(vg);
		bb::hazard(vg, Rect(Vec(2.4f, 79.6f), Vec(46.0f, 1.4f)), k, 1.8f);

		std::shared_ptr<window::Font> stencil = pluginFont("BlackOpsOne-Regular.ttf");
		std::shared_ptr<window::Font> bold = pluginFont("SpaceMono-Bold.ttf");
		if (!stencil || !bold)
			return;
		bb::title(vg, Rect(Vec(11.4f, 1.6f), Vec(36.4f, 6.4f)), "KOBULETI", stencil->handle, k, 4.0f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		// mode labels under the four LEDs
		nvgFontFaceId(vg, bold->handle);
		nvgFontSize(vg, mm2px(2.4f));
		nvgFillColor(vg, k.fg);
		static const char* const modes[4] = {"FREE", "QUAD", "PHASE", "DIV"};
		for (int i = 0; i < 4; i++)
			nvgText(vg, mm2px(19.0f + i * 7.8f), mm2px(27.3f), modes[i], NULL);

		nvgFontFaceId(vg, stencil->handle);
		nvgTextLetterSpacing(vg, mm2px(0.1f));
		// channel numbers on solid plates, under the knobs
		nvgFontSize(vg, mm2px(3.2f));
		for (int i = 0; i < 4; i++)
			bb::plate(vg, Vec(X[i], 48.5f), string::f("%d", i + 1).c_str(), k, 6.4f, 4.0f);

		nvgFontSize(vg, mm2px(2.8f));
		bb::plate(vg, Vec(25.4f, 64.0f), "CV", k, 9.0f);
		bb::plate(vg, Vec(25.4f, 75.6f), "RESET / SYNC", k, 9.0f);
		bb::plate(vg, Vec(25.4f, 97.8f), "SINE", inv, 9.0f);
		bb::plate(vg, Vec(25.4f, 109.4f), "ASSIGN", inv, 9.0f);

		// switches: title above, the two states on the right
		static const struct {
			float x;
			const char* title;
			const char* top;
			const char* bottom;
		} sw[3] = {{7.2f, "SYNC", "SYNC", "RESET"}, {22.4f, "WAVE 1", "ON", "OFF"}, {37.6f, "WAVE 2", "ON", "OFF"}};
		for (const auto& w : sw) {
			nvgFontFaceId(vg, stencil->handle);
			nvgFontSize(vg, mm2px(2.4f));
			bb::plate(vg, Vec(w.x + 2.6f, 115.6f), w.title, k, 10.0f, 2.8f);
			nvgFontFaceId(vg, bold->handle);
			nvgFontSize(vg, mm2px(2.3f));
			nvgFillColor(vg, k.fg);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgText(vg, mm2px(w.x + 4.2f), mm2px(119.0f), w.top, NULL);
			nvgText(vg, mm2px(w.x + 4.2f), mm2px(122.2f), w.bottom, NULL);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		}
		nvgTextLetterSpacing(vg, 0.f);
	}
};

struct KobuletiWidget : ModuleWidget {
	KobuletiWidget(Kobuleti* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Kobuleti.svg")));

		PanelArt* art = new PanelArt;
		art->module = module;
		art->box.size = box.size;
		addChild(art);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const float X[4] = {8.2f, 19.6f, 31.0f, 42.4f};

		// select button and the four mode LEDs
		addParam(createParamCentered<fms::Button>(mm2px(Vec(X[0], 24.4f)), module, Kobuleti::SELECT_PARAM));
		for (int i = 0; i < 4; i++) {
			auto* led = createLightCentered<SmallLight<GrayModuleLightWidget>>(mm2px(Vec(19.0f + i * 7.8f, 22.6f)), module, Kobuleti::MODE_LIGHT + i);
			led->addBaseColor(LED_POSITIVE);
			addChild(led);
		}

		for (int i = 0; i < 4; i++) {
			addParam(fms::knob(mm2px(Vec(X[i], 40.2f)), module, Kobuleti::FREQ_PARAM + i, 3.9f));
			addInput(createInputCentered<fms::Jack>(mm2px(Vec(X[i], 58.2f)), module, Kobuleti::CV_INPUT + i));
			addInput(createInputCentered<fms::Jack>(mm2px(Vec(X[i], 70.4f)), module, Kobuleti::RESET_INPUT + i));
			// positive half white, negative half grey
			auto* led = createLightCentered<MediumLight<GrayModuleLightWidget>>(mm2px(Vec(X[i], 83.6f)), module, Kobuleti::CHANNEL_LIGHT + 2 * i);
			led->addBaseColor(LED_POSITIVE);
			led->addBaseColor(LED_NEGATIVE);
			addChild(led);
			addOutput(createOutputCentered<fms::Jack>(mm2px(Vec(X[i], 92.8f)), module, Kobuleti::SINE_OUTPUT + i));
			addOutput(createOutputCentered<fms::Jack>(mm2px(Vec(X[i], 104.4f)), module, Kobuleti::ASGN_OUTPUT + i));
		}

		static const struct {
			float x;
			int id;
		} sw[] = {{7.2f, Kobuleti::SYNC_PARAM}, {22.4f, Kobuleti::WAV1_PARAM}, {37.6f, Kobuleti::WAV2_PARAM}};
		for (const auto& s : sw)
			addParam(createParamCentered<fms::Toggle>(mm2px(Vec(s.x, 120.6f)), module, s.id));
	}

	void appendContextMenu(Menu* menu) override {
		Kobuleti* m = getModule<Kobuleti>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Theme", std::vector<std::string>(THEME_NAMES, THEME_NAMES + NUM_THEMES), &m->theme));
	}
};

Model* modelKobuleti = createModel<Kobuleti, KobuletiWidget>("Kobuleti");

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelKobuleti);
}
