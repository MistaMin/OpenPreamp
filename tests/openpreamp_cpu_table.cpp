// Repeatable processor-only CPU measurements. Input construction/copying and
// prepareToPlay are outside timing. No editor is opened by this harness.
#include "OpenPreampPlugin/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
struct Options {
    bool quick = false, referenceLight = false, referenceCircuit = false;
    double seconds = 0.25;
    int repeats = 3, model = -1, mode = -1, factor = 0;
    std::string wav;
};
struct Result { double cpu, worstUs; int overruns; };
void parameter(OpenPreampProcessor& p, const char* id, float value) {
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(value));
}
std::vector<float> makeInput(double rate, int frames, int workload, const juce::AudioBuffer<float>& recording, double recordingRate) {
    std::vector<float> input(size_t(2 * frames));
    juce::Random random(7);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < frames; ++i) {
        const double t = i / rate;
        double x = 0;
        if (workload == 1) x = 0.1 * std::sin(2 * juce::MathConstants<double>::pi * 997 * t + c * 0.3);
        if (workload == 2) x = 0.95 * std::sin(2 * juce::MathConstants<double>::pi * 997 * t + c * 0.3);
        if (workload == 3) x = 0.4 * (std::sin(2 * juce::MathConstants<double>::pi * 60 * t) + std::sin(2 * juce::MathConstants<double>::pi * 7000 * t + c * 0.3));
        if (workload == 4) x = 0.5 * (2 * random.nextFloat() - 1);
        if (workload == 5) {
            const double pos = std::fmod(t * recordingRate, double(recording.getNumSamples()));
            const int a = int(pos), b = (a + 1) % recording.getNumSamples();
            const int channel = std::min(c, recording.getNumChannels() - 1);
            x = recording.getSample(channel, a) * (1 - (pos - a)) + recording.getSample(channel, b) * (pos - a);
        }
        input[size_t(c * frames + i)] = float(x);
    }
    return input;
}
Result measure(OpenPreampProcessor& p, double rate, int block, const std::vector<float>& input, int frames) {
    juce::AudioBuffer<float> buffer(2, block);
    juce::MidiBuffer midi;
    const int count = frames / block;
    auto feed = [&](int offset) {
        for (int c = 0; c < 2; ++c) buffer.copyFrom(c, 0, input.data() + c * frames + offset, block);
    };
    // Warm up for at least 50 ms; timing does not include configuration or allocations.
    for (int i = 0; i < std::max(1, int(std::ceil(rate * 0.05 / block))); ++i) {
        feed((i % count) * block); p.processBlock(buffer, midi);
    }
    double total = 0, worst = 0;
    int overruns = 0;
    const double deadline = block / rate;
    for (int i = 0; i < count; ++i) {
        feed(i * block);
        const auto start = Clock::now();
        p.processBlock(buffer, midi);
        const double duration = std::chrono::duration<double>(Clock::now() - start).count();
        total += duration; worst = std::max(worst, duration);
        if (duration > deadline) ++overruns;
    }
    return {100 * total / (frames / rate), worst * 1e6, overruns};
}
}
int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--quick") options.quick = true;
        else if (arg == "--reference-light") options.referenceLight = true;
        else if (arg == "--reference-circuit") options.referenceCircuit = true;
        else if (arg == "--help") {
            std::puts("OpenPreampCPU [--quick] [--seconds N] [--repeats N] [--model 0|1|2|3|4] [--mode 0|1|2] [--factor 1|2|4] [--input-wav PATH] [--reference-light] [--reference-circuit]\nModes: 0=Standard 1x, 1=Circuit 2x, 2=Circuit 4x; --factor overrides rate; --reference-circuit selects HQ. Circuit/Standard use ADAA only at 1x. Quick: 48k/64, silence/driven tone; full: 48/96k, 32/64/256, five workloads plus optional recording."); return 0;
        } else if (i + 1 < argc && arg == "--seconds") options.seconds = std::atof(argv[++i]);
        else if (i + 1 < argc && arg == "--repeats") options.repeats = std::atoi(argv[++i]);
        else if (i + 1 < argc && arg == "--model") options.model = std::atoi(argv[++i]);
        else if (i + 1 < argc && arg == "--mode") options.mode = std::atoi(argv[++i]);
        else if (i + 1 < argc && arg == "--factor") options.factor = std::atoi(argv[++i]);
        else if (i + 1 < argc && arg == "--input-wav") options.wav = argv[++i];
        else { std::fprintf(stderr, "Unknown/incomplete argument: %s\n", arg.c_str()); return 2; }
    }
    if (!std::isfinite(options.seconds) || options.seconds < 0.01 || options.seconds > 60 || options.repeats < 1 || options.repeats > 100 || options.model < -1 || options.model > 4 || options.mode < -1 || options.mode > 2) return 2;
    if(options.factor!=0 && options.factor!=1 && options.factor!=2 && options.factor!=4)return 2;
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioBuffer<float> recording;
    double recordingRate = 0;
    if (!options.wav.empty()) {
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(juce::String(options.wav))));
        if (!reader || reader->lengthInSamples < 2 || reader->lengthInSamples > 100000000) { std::fprintf(stderr, "Cannot read input recording\n"); return 2; }
        recording.setSize(2, int(reader->lengthInSamples));
        if (!reader->read(&recording, 0, recording.getNumSamples(), 0, true, true)) return 2;
        recordingRate = reader->sampleRate;
    }
    const std::vector<double> rates = options.quick ? std::vector<double>{48000} : std::vector<double>{48000, 96000};
    const std::vector<int> blocks = options.quick ? std::vector<int>{64} : std::vector<int>{32, 64, 256};
    std::vector<int> workloads = options.quick ? std::vector<int>{0, 2} : std::vector<int>{0, 1, 2, 3, 4};
    if (recording.getNumSamples()) workloads.push_back(5);
    const char* names[] = {"Brit", "N-Type", "FSF", "Off", "A-Type"};
    const char* signals[] = {"silence", "tone", "driven-tone", "two-tone", "noise", "recording"};
    std::printf("# OpenPreamp %s, stereo, Release recommended, single-thread wall time / audio duration; GUI excluded; scheduler/core not pinned\n", JucePlugin_VersionString);
    std::printf("# Light curve tables: %s; input/output trims 0 dB, Unity PAD, cuts on at 20 Hz/20 kHz, mono maker off; warm-up 50 ms per repeat\n", options.referenceLight ? "off (reference)" : "on (candidate)");
    std::puts("rate,block,model,mode,signal,median_cpu_percent,min_cpu_percent,max_cpu_percent,worst_block_us,deadline_overruns,measured_blocks,repeats"); std::fflush(stdout);
    OpenPreampProcessor p;
    parameter(p,"circuitEngine",options.referenceCircuit ? 0.0f : 1.0f);
    for (double rate : rates) for (int block : blocks) {
        const int frames = std::max(block, int(std::ceil(options.seconds * rate / block)) * block);
        p.setPlayConfigDetails(2, 2, rate, block); p.prepareToPlay(rate, block);
        p.setLightTablesEnabled(!options.referenceLight);
        for (int model : {0, 1, 2, 3, 4}) {
            if (options.model >= 0 && options.model != model) continue;
            for (int mode = 0; mode < (model == 3 ? 1 : 3); ++mode) {
                if (options.mode >= 0 && options.mode != mode) continue;
                parameter(p, "preampType", float(model)); parameter(p, "preampCircuit", mode > 0 ? 1.0f : 0.0f); parameter(p, "hqMode", mode == 2 ? 1.0f : 0.0f);
                const int factor=options.factor?options.factor:mode==0?1:mode==1?2:4;
                parameter(p,"preampOversampling",factor==1?1.f:factor==2?2.f:3.f);
                const std::string modeName=model==3?"Off":std::string(mode==0?"Standard":options.referenceCircuit?"HQ":"Circuit")+std::to_string(factor)+"x";
                for (int workload : workloads) {
                    const auto input = makeInput(rate, frames, workload, recording, recordingRate);
                    std::vector<double> cpus; double worst = 0; int overruns = 0;
                    for (int repeat = 0; repeat < options.repeats; ++repeat) {
                        // Continuous steady-state repetitions; each is independently warmed.
                        // Model/rate configuration occurs in the untimed warm-up.
                        const auto result = measure(p, rate, block, input, frames);
                        cpus.push_back(result.cpu); worst = std::max(worst, result.worstUs); overruns += result.overruns;
                    }
                    std::sort(cpus.begin(), cpus.end());
                    const double median = (cpus[size_t((options.repeats - 1) / 2)] + cpus[size_t(options.repeats / 2)]) * 0.5;
                    std::printf("%.0f,%d,%s,%s,%s,%.4f,%.4f,%.4f,%.3f,%d,%d,%d\n", rate, block, names[model], modeName.c_str(), signals[workload], median, cpus.front(), cpus.back(), worst, overruns, frames / block * options.repeats, options.repeats); std::fflush(stdout);
                }
            }
        }
    }
    return 0;
}
