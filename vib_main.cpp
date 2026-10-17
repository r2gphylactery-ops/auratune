#define main old_main
#include "engine_test.cpp"
#undef main
int main(){ double sr=44100; for(float rt: {0.f,20.f,50.f,150.f}) for (float soft : {0.f, 0.5f, 1.f}) {
  aura::TuneEngine e; e.prepare(sr); aura::TuneParams p; p.retuneMs=rt; p.softness=soft; p.humanize=0; e.setParams(p);
  auto in=voice(sr,2.0,440.0,35); auto out=run(e,in); int L=e.getLatencySamples();
  double mn=1e9,mx=-1e9, mni=1e9,mxi=-1e9;
  for(double t=1.0;t<1.8;t+=0.01){ size_t i=(size_t)(t*sr); double f=measurePitch(out,i,(size_t)(0.012*sr),sr); double c=cents(f,440); mn=std::min(mn,c); mx=std::max(mx,c);
     double fi=measurePitch(in,i-L,(size_t)(0.012*sr),sr); double ci=cents(fi,440); mni=std::min(mni,ci); mxi=std::max(mxi,ci);}
  printf("retune %3.0f soft %.1f: in vib %+.0f..%+.0f c  -> out %+.0f..%+.0f c\n",rt,soft,mni,mxi,mn,mx);} }
