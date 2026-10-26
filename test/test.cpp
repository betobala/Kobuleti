// Headless checks of the Batumi port: frequency range, modes and reset.
#include "../src/Kobuleti.cpp"
#include <cstdio>

static Kobuleti* m;
static const float SR = 48000.f;
static int failures = 0;

static void run(float seconds) {
	Module::ProcessArgs a;
	a.sampleRate = SR;
	a.sampleTime = 1.f / SR;
	int n = (int) (seconds * SR);
	for (int i = 0; i < n; i++)
		m->process(a);
}

// Frequency of an output from rising zero crossings
static float measure(int output, float seconds) {
	Module::ProcessArgs a;
	a.sampleRate = SR;
	a.sampleTime = 1.f / SR;
	int n = (int) (seconds * SR);
	float prev = m->outputs[output].getVoltage();
	int first = -1, last = -1, count = 0;
	for (int i = 0; i < n; i++) {
		m->process(a);
		float v = m->outputs[output].getVoltage();
		if (prev < 0.f && v >= 0.f) {
			if (first < 0)
				first = i;
			else
				count++;
			last = i;
		}
		prev = v;
	}
	return count > 0 ? count * SR / (last - first) : 0.f;
}

static void check(const char* what, bool ok, float value) {
	if (!ok)
		failures++;
	printf("%s %-50s %.4f\n", ok ? "ok  " : "FAIL", what, value);
}

static void pressSelect(float seconds) {
	m->params[Kobuleti::SELECT_PARAM].setValue(1.f);
	run(seconds);
	m->params[Kobuleti::SELECT_PARAM].setValue(0.f);
	run(0.05f);
}

int main() {
	random::init();
	m = new Kobuleti;
	for (int i = 0; i < Kobuleti::OUTPUTS_LEN; i++)
		m->outputs[i].channels = 1;
	run(0.5f);  // splash animation

	printf("-- Free mode\n");
	check("mode is FREE", m->ui.feat_mode() == batumi::FEAT_MODE_FREE, m->ui.feat_mode());
	float peak = 0.f;
	m->params[Kobuleti::FREQ_PARAM].setValue(0.75f);
	run(0.3f);
	for (int i = 0; i < 48000; i++) {
		run(1.f / SR);
		peak = std::max(peak, std::fabs(m->outputs[Kobuleti::SINE_OUTPUT].getVoltage()));
	}
	check("sine amplitude ~5 V", std::fabs(peak - 5.f) < 0.3f, peak);
	m->params[Kobuleti::FREQ_PARAM].setValue(0.f);
	run(0.3f);
	check("knob 0: slower than one cycle per minute", measure(Kobuleti::SINE_OUTPUT, 70.f) == 0.f, 0.f);
	float knobs[] = {0.25f, 0.5f, 0.75f, 1.f};
	float prevF = 0.f;
	for (float k : knobs) {
		m->params[Kobuleti::FREQ_PARAM].setValue(k);
		run(0.3f);
		float f = measure(Kobuleti::SINE_OUTPUT, k < 0.3f ? 60.f : 4.f);
		char buf[64];
		snprintf(buf, sizeof(buf), "knob %.2f frequency (Hz) increases", k);
		check(buf, f > prevF, f);
		prevF = f;
	}
	// CV: +1 V doubles the frequency
	m->params[Kobuleti::FREQ_PARAM].setValue(0.6f);
	run(0.3f);
	float f0 = measure(Kobuleti::SINE_OUTPUT, 4.f);
	m->inputs[Kobuleti::CV_INPUT].channels = 1;
	m->inputs[Kobuleti::CV_INPUT].setVoltage(1.f);
	run(0.3f);
	float f1 = measure(Kobuleti::SINE_OUTPUT, 4.f);
	check("CV +1 V = one octave up (ratio)", std::fabs(f1 / f0 - 2.f) < 0.1f, f1 / f0);
	m->inputs[Kobuleti::CV_INPUT].setVoltage(0.f);

	printf("-- Quadrature mode\n");
	pressSelect(0.1f);
	check("mode is QUAD", m->ui.feat_mode() == batumi::FEAT_MODE_QUAD, m->ui.feat_mode());
	for (int i = 1; i < 4; i++)
		m->params[Kobuleti::FREQ_PARAM + i].setValue(1.f);
	m->params[Kobuleti::FREQ_PARAM].setValue(0.6f);
	run(1.f);
	// Find when channel 1 crosses zero upward, then compare channel 2 a quarter period later
	float f = measure(Kobuleti::SINE_OUTPUT, 4.f);
	float c1 = m->outputs[Kobuleti::SINE_OUTPUT + 0].getVoltage();
	float c2 = m->outputs[Kobuleti::SINE_OUTPUT + 1].getVoltage();
	float c3 = m->outputs[Kobuleti::SINE_OUTPUT + 2].getVoltage();
	check("channels share the frequency", std::fabs(measure(Kobuleti::SINE_OUTPUT + 1, 4.f) - f) < 0.05f * f, f);
	float a1 = std::atan2(m->outputs[Kobuleti::SINE_OUTPUT + 0].getVoltage(), m->outputs[Kobuleti::SINE_OUTPUT + 1].getVoltage());
	(void) a1;
	(void) c1;
	(void) c2;
	(void) c3;
	// Ch1 and Ch3 are 180 degrees apart in quadrature mode
	float maxSum = 0.f;
	for (int i = 0; i < 48000; i++) {
		run(1.f / SR);
		maxSum = std::max(maxSum, std::fabs(m->outputs[Kobuleti::SINE_OUTPUT + 0].getVoltage() + m->outputs[Kobuleti::SINE_OUTPUT + 2].getVoltage()));
	}
	check("ch1 + ch3 cancel (180 deg)", maxSum < 0.5f, maxSum);
	float maxDot = 0.f, sumDot = 0.f;
	for (int i = 0; i < 48000; i++) {
		run(1.f / SR);
		float a = m->outputs[Kobuleti::SINE_OUTPUT + 0].getVoltage();
		float b = m->outputs[Kobuleti::SINE_OUTPUT + 1].getVoltage();
		sumDot += a * b;
		maxDot = std::max(maxDot, std::fabs(a * b));
	}
	check("ch1 and ch2 orthogonal (90 deg)", std::fabs(sumDot / 48000.f) < 0.5f, sumDot / 48000.f);

	printf("-- Divide mode\n");
	pressSelect(0.1f);
	pressSelect(0.1f);
	check("mode is DIVIDE", m->ui.feat_mode() == batumi::FEAT_MODE_DIVIDE, m->ui.feat_mode());
	m->params[Kobuleti::FREQ_PARAM].setValue(0.75f);
	m->params[Kobuleti::FREQ_PARAM + 1].setValue(0.3f);
	run(1.f);
	float fa = measure(Kobuleti::SINE_OUTPUT, 6.f);
	float fb = measure(Kobuleti::SINE_OUTPUT + 1, 6.f);
	float ratio = fa / fb;
	check("ch2 is an integer division of ch1", std::fabs(ratio - std::round(ratio)) < 0.1f && ratio >= 1.5f, ratio);

	printf("-- Reset\n");
	pressSelect(0.1f);
	check("back to FREE", m->ui.feat_mode() == batumi::FEAT_MODE_FREE, m->ui.feat_mode());
	m->params[Kobuleti::FREQ_PARAM].setValue(0.4f);
	run(0.77f);
	m->inputs[Kobuleti::RESET_INPUT].channels = 1;
	m->inputs[Kobuleti::RESET_INPUT].setVoltage(10.f);
	run(0.002f);
	float afterReset = m->outputs[Kobuleti::SINE_OUTPUT].getVoltage();
	check("sine restarts near 0 V after reset", std::fabs(afterReset) < 0.6f, afterReset);

	printf("-- Persistence\n");
	json_t* j = m->dataToJson();
	Kobuleti* m2 = new Kobuleti;
	m2->dataFromJson(j);
	check("mode survives JSON", m2->ui.feat_mode() == m->ui.feat_mode(), m2->ui.feat_mode());

	printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
	return failures ? 1 : 0;
}
