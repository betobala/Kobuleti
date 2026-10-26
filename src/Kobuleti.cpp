// Kobuleti: VCV Rack host for the open source Batumi firmware (MIT, Matthias Puech).
// The firmware's Lfo, Processor and lookup tables run unchanged at their native
// 16384 Hz rate; this file replaces the STM32 drivers with Rack's ports and params.
#include <rack.hpp>
#include <cstring>
#include "Controls4ms.hpp"
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
// Palette "Bone" (light) and "Slate" (dark), one accent (ultramarine). See docs/art/STYLE.md.
struct KTheme {
	NVGcolor panel, ink, grey, frame, group, plate, plateInk, plateLine, accent, negative;
};
static const int NUM_THEMES = 2;
static const char* const THEME_NAMES[NUM_THEMES] = {"Bone", "Slate"};

static KTheme kTheme(int t) {
	KTheme k;
	if (t == 1) {
		k.panel = nvgRGB(0x17, 0x19, 0x1B);
		k.ink = nvgRGB(0xE6, 0xE8, 0xE8);
		k.grey = nvgRGB(0x7C, 0x7F, 0x85);
		k.frame = nvgRGB(0x6B, 0x6F, 0x75);
		k.group = nvgRGB(0x1E, 0x21, 0x24);
		k.plate = nvgRGB(0x0E, 0x11, 0x13);
		k.plateInk = nvgRGB(0xE6, 0xE8, 0xE8);
		k.plateLine = nvgRGB(0x6B, 0x6F, 0x75);
		k.accent = nvgRGB(0x5B, 0x74, 0xFF);
	}
	else {
		k.panel = nvgRGB(0xE8, 0xE6, 0xE1);
		k.ink = nvgRGB(0x0F, 0x12, 0x16);
		k.grey = nvgRGB(0x7C, 0x7F, 0x85);
		k.frame = nvgRGB(0x0F, 0x12, 0x16);
		k.group = nvgRGB(0xF3, 0xF1, 0xEC);
		k.plate = nvgRGB(0x1B, 0x1E, 0x24);
		k.plateInk = nvgRGB(0xF6, 0xF4, 0xEF);
		k.plateLine = nvgRGB(0x1B, 0x1E, 0x24);
		k.accent = nvgRGB(0x2F, 0x4D, 0xFF);
	}
	k.negative = nvgRGB(0xF2, 0xC9, 0x4C);
	return k;
}

namespace fms {
const Colors& colors(const engine::Module* m) {
	static const Colors bone = {
		nvgRGB(0x11, 0x11, 0x11), nvgRGB(0x3A, 0x3A, 0x3A), nvgRGB(0xF6, 0xF4, 0xEF), nvgRGB(0x0F, 0x12, 0x16),
		nvgRGB(0xD3, 0xD3, 0xD0), nvgRGB(0xF7, 0xF7, 0xF5), nvgRGB(0x8E, 0x8E, 0x8A), nvgRGB(0x5C, 0x5C, 0x5A), nvgRGB(0x05, 0x05, 0x05),
		nvgRGB(0xF4, 0xF2, 0xEA), nvgRGB(0xFF, 0xFF, 0xFF), nvgRGB(0x2F, 0x4D, 0xFF), nvgRGB(0x7C, 0x90, 0xFF), nvgRGB(0x0F, 0x12, 0x16),
		nvgRGB(0xFB, 0xFB, 0xF9)};
	static const Colors slate = {
		nvgRGB(0x0B, 0x0B, 0x0D), nvgRGB(0x3A, 0x3B, 0x3F), nvgRGB(0xF6, 0xF4, 0xEF), nvgRGB(0xE6, 0xE8, 0xE8),
		nvgRGB(0xD3, 0xD3, 0xD0), nvgRGB(0xF7, 0xF7, 0xF5), nvgRGB(0x8E, 0x8E, 0x8A), nvgRGB(0x5C, 0x5C, 0x5A), nvgRGB(0x05, 0x05, 0x05),
		nvgRGB(0xE6, 0xE8, 0xE8), nvgRGB(0xFF, 0xFF, 0xFF), nvgRGB(0x5B, 0x74, 0xFF), nvgRGB(0x9A, 0xAA, 0xFF), nvgRGB(0x6B, 0x6F, 0x75),
		nvgRGB(0xFB, 0xFB, 0xF9)};
	int t = m ? static_cast<const Kobuleti*>(m)->theme : 0;
	return t == 1 ? slate : bone;
}
}  // namespace fms

static std::shared_ptr<window::Font> pluginFont(const char* file) {
	std::shared_ptr<window::Font> f = APP->window->loadFont(asset::plugin(pluginInstance, std::string("res/fonts/") + file));
	if (!f)
		f = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
	return f;
}

// ---------------------------------------------------------------- widgets
/** The logotype, one SVG per theme (bone / slate). */
struct LogoWidget : TransparentWidget {
	Kobuleti* module = nullptr;
	std::shared_ptr<window::Svg> svg[2];

	void load(float widthMm) {
		svg[0] = window::Svg::load(asset::plugin(pluginInstance, "res/logo_bone.svg"));
		svg[1] = window::Svg::load(asset::plugin(pluginInstance, "res/logo_slate.svg"));
		float w = svg[0]->handle->width, h = svg[0]->handle->height;
		box.size = Vec(mm2px(widthMm), mm2px(widthMm) * h / w);
	}

	void draw(const DrawArgs& args) override {
		int i = (module && module->theme == 1) ? 1 : 0;
		if (!svg[i])
			return;
		float sc = box.size.x / svg[i]->handle->width;
		nvgSave(args.vg);
		nvgScale(args.vg, sc, sc);
		window::svgDraw(args.vg, svg[i]->handle);
		nvgRestore(args.vg);
	}
};

/** Panel background, rounded groups and every label. Drawn in code so the theme can change. */
struct PanelArt : Widget {
	Kobuleti* module = nullptr;

	void group(NVGcontext* vg, float x, float y, float w, float h, NVGcolor fill, NVGcolor line, float lineMm) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(w), mm2px(h), mm2px(2.6f));
		nvgFillColor(vg, fill);
		nvgFill(vg);
		if (lineMm > 0.f) {
			nvgStrokeColor(vg, line);
			nvgStrokeWidth(vg, mm2px(lineMm));
			nvgStroke(vg);
		}
	}

	void draw(const DrawArgs& args) override {
		KTheme k = kTheme(module ? module->theme : 0);
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, box.size.x, box.size.y);
		nvgFillColor(vg, k.panel);
		nvgFill(vg);

		// groups: select + modes, the two input rows, the outputs on a dark plate, the switches
		group(vg, 2.4f, 18.2f, 46.0f, 12.4f, k.group, k.frame, 0.35f);
		group(vg, 2.4f, 52.2f, 46.0f, 26.8f, k.group, k.frame, 0.35f);
		group(vg, 2.4f, 87.0f, 46.0f, 26.4f, k.plate, k.plateLine, 0.3f);

		std::shared_ptr<window::Font> reg = pluginFont("SpaceMono-Regular.ttf");
		std::shared_ptr<window::Font> bold = pluginFont("SpaceMono-Bold.ttf");
		if (!reg || !bold)
			return;
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		const float X[4] = {8.2f, 19.6f, 31.0f, 42.4f};

		// mode labels under the four LEDs
		nvgFontFaceId(vg, reg->handle);
		nvgFontSize(vg, mm2px(2.7f));
		nvgFillColor(vg, k.ink);
		static const char* const modes[4] = {"FREE", "QUAD", "PHASE", "DIV"};
		for (int i = 0; i < 4; i++)
			nvgText(vg, mm2px(19.0f + i * 7.8f), mm2px(27.3f), modes[i], NULL);

		// channel numbers in black pills, under the knobs
		nvgFontFaceId(vg, bold->handle);
		nvgFontSize(vg, mm2px(3.4f));
		for (int i = 0; i < 4; i++) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(X[i] - 3.3f), mm2px(48.5f - 2.1f), mm2px(6.6f), mm2px(4.2f), mm2px(2.1f));
			nvgFillColor(vg, k.plate);
			nvgFill(vg);
			nvgFillColor(vg, k.plateInk);
			nvgText(vg, mm2px(X[i]), mm2px(48.5f), string::f("%d", i + 1).c_str(), NULL);
		}

		nvgFontFaceId(vg, reg->handle);
		nvgFontSize(vg, mm2px(3.3f));
		nvgFillColor(vg, k.ink);
		nvgText(vg, mm2px(25.4f), mm2px(64.0f), "CV", NULL);
		nvgText(vg, mm2px(25.4f), mm2px(75.6f), "RESET / SYNC", NULL);
		nvgFillColor(vg, k.plateInk);
		nvgText(vg, mm2px(25.4f), mm2px(97.8f), "SINE", NULL);
		nvgText(vg, mm2px(25.4f), mm2px(109.4f), "ASSIGN", NULL);

		// switches: title above, the two states on the right
		nvgFillColor(vg, k.ink);
		nvgFontSize(vg, mm2px(2.9f));
		static const struct {
			float x;
			const char* title;
			const char* top;
			const char* bottom;
		} sw[3] = {{7.2f, "SYNC", "SYNC", "RESET"}, {22.4f, "WAVE 1", "ON", "OFF"}, {37.6f, "WAVE 2", "ON", "OFF"}};
		for (const auto& w : sw) {
			nvgText(vg, mm2px(w.x + 2.6f), mm2px(115.6f), w.title, NULL);
			nvgFillColor(vg, k.grey);
			nvgFontSize(vg, mm2px(2.6f));
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgText(vg, mm2px(w.x + 4.2f), mm2px(119.0f), w.top, NULL);
			nvgText(vg, mm2px(w.x + 4.2f), mm2px(122.2f), w.bottom, NULL);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, k.ink);
			nvgFontSize(vg, mm2px(2.9f));
		}
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

		LogoWidget* logo = new LogoWidget;
		logo->module = module;
		logo->load(27.0f);
		logo->box.pos = Vec((box.size.x - logo->box.size.x) / 2, mm2px(4.0f));
		addChild(logo);

		const float X[4] = {8.2f, 19.6f, 31.0f, 42.4f};
		KTheme k = kTheme(0);

		// select button and the four mode LEDs
		addParam(createParamCentered<fms::Button>(mm2px(Vec(X[0], 24.4f)), module, Kobuleti::SELECT_PARAM));
		for (int i = 0; i < 4; i++) {
			auto* led = createLightCentered<SmallLight<GrayModuleLightWidget>>(mm2px(Vec(19.0f + i * 7.8f, 22.6f)), module, Kobuleti::MODE_LIGHT + i);
			led->addBaseColor(k.accent);
			addChild(led);
		}

		for (int i = 0; i < 4; i++) {
			addParam(fms::knob(mm2px(Vec(X[i], 40.2f)), module, Kobuleti::FREQ_PARAM + i, 3.9f));
			addInput(createInputCentered<fms::Jack>(mm2px(Vec(X[i], 58.2f)), module, Kobuleti::CV_INPUT + i));
			addInput(createInputCentered<fms::Jack>(mm2px(Vec(X[i], 70.4f)), module, Kobuleti::RESET_INPUT + i));
			// positive half in the accent colour, negative half in amber
			auto* led = createLightCentered<MediumLight<GrayModuleLightWidget>>(mm2px(Vec(X[i], 83.6f)), module, Kobuleti::CHANNEL_LIGHT + 2 * i);
			led->addBaseColor(k.accent);
			led->addBaseColor(k.negative);
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
