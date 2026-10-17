#define main old_main
#include "engine_test.cpp"
#undef main
#include <chrono>
int main(){ for (double sr : {44100.0, 48000.0, 96000.0}) { aura::TuneEngine e; e.prepare(sr); aura::TuneParams p; p.inputType=3; e.setParams(p);
 auto in=voice(sr,10.0,220.0,30); auto t0=std::chrono::steady_clock::now(); auto out=run(e,in); double s=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count();
 printf("sr %.0f: 10 s of audio in %.3f s -> %.1f%% of one core\n", sr, s, s*10); } }
