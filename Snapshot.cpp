// Dev tool: renders the editor to PNG while a synthetic vocal plays (no audio device needed).
#include "../Source/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <atomic>
using namespace juce;
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    AuraTuneProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    proc.apvts.getParameter ("scale")->setValueNotifyingHost (proc.apvts.getParameter ("scale")->convertTo0to1 (1));
    proc.apvts.getParameter ("noteOff11")->setValueNotifyingHost (1.0f);
    std::atomic<bool> run { true };
    std::thread audio ([&] {
        AudioBuffer<float> b (2, 512); MidiBuffer m; double ph = 0; long long t = 0;
        const double notes[] = { 261.0, 300.0, 335.0, 386.0, 345.0, 300.0 };
        while (run) {
            for (int i = 0; i < 512; ++i, ++t) {
                const double sec = t / 44100.0; const int idx = (int) (sec / 0.7) % 6;
                const double f = notes[idx] * std::pow (2.0, 0.35 / 12 * std::sin (2 * 3.14159 * 5.5 * sec));
                ph += f / 44100.0; double s = 0;
                for (int h = 1; h < 12; ++h) s += std::sin (2 * 3.14159 * h * ph) / h;
                const float v = (float) (0.25 * s * (std::fmod (sec, 0.7) < 0.6 ? 1.0 : 0.0));
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            proc.processBlock (b, m);
            std::this_thread::sleep_for (std::chrono::microseconds (11000));
        }
    });
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    MessageManager::getInstance()->runDispatchLoopUntil (argc > 2 ? atoi (argv[2]) : 6000);
    run = false; audio.join();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    File out (argc > 1 ? argv[1] : "snapshot.png"); out.deleteFile();
    FileOutputStream os (out); PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
