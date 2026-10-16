#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// OpenPreamp's vector faceplate. Opted into per component so shared HybridEQ
// controls and other open plugin instances never inherit a global skin change.
namespace PreampChrome {
inline const juce::Colour ink{0xffe9e9df}, muted{0xffa2b5c4}, accent{0xffae3839};
inline const juce::Colour top{0xff1a3042}, bottom{0xff0d1b28}, rim{0xff536c7c};
inline void panel(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setColour(juce::Colour(0x90000000)); g.fillRoundedRectangle(r.translated(0,1),6);
    g.setGradientFill(juce::ColourGradient(top,r.getX(),r.getY(),bottom,r.getRight(),r.getBottom(),false));
    g.fillRoundedRectangle(r,6);
    g.setColour(juce::Colour(0x03ffffff));
    for(float y=r.getY()+3;y<r.getBottom()-3;y+=3) g.drawHorizontalLine(int(y),r.getX()+4,r.getRight()-4);
    g.setColour(juce::Colour(0xff030c13)); g.drawRoundedRectangle(r,6,1.6f);
    g.setColour(rim.withAlpha(.65f)); g.drawRoundedRectangle(r.reduced(1),5,0.65f);
}
inline void key(juce::Graphics& g, juce::Rectangle<float> r, bool selected, bool enabled,
                bool over=false, bool down=false, bool toggle=false) {
    auto hi=selected ? (toggle?accent:juce::Colour(0xffedead8)) : juce::Colour(0xff253b4b);
    auto lo=selected ? (toggle?juce::Colour(0xff702327):juce::Colour(0xffbcbdaa)) : juce::Colour(0xff10212e);
    if(!enabled) {hi=juce::Colour(0xff25313a);lo=juce::Colour(0xff18222a);}
    if(over)hi=hi.brighter(.08f);
    if(down){hi=hi.darker(.15f);lo=lo.darker(.15f);}
    g.setColour(juce::Colour(0xff02090e));g.fillRoundedRectangle(r.expanded(1),3);
    g.setGradientFill(juce::ColourGradient(hi,r.getX(),r.getY(),lo,r.getX(),r.getBottom(),false));
    g.fillRoundedRectangle(r,2.5f);
    g.setColour((selected?juce::Colours::white:rim).withAlpha(enabled?.5f:.2f));g.drawRoundedRectangle(r.reduced(.5f),2,0.7f);
}
inline juce::Colour keyText(bool selected,bool enabled,bool toggle=false) {
    return !enabled ? juce::Colour(0xff758693) : selected && !toggle ? juce::Colour(0xff17212a) : ink;
}
inline void link(juce::Graphics& g,juce::Point<float> c, bool enabled) {
    const auto colour=enabled?accent.brighter(.3f):muted.withAlpha(.2f);
    g.setColour(colour);
    for(float dx : {-3.5f,3.5f}) {
        juce::Path p;p.addRoundedRectangle(-5,-3,10,6,3);
        p.applyTransform(juce::AffineTransform::rotation(-.55f).translated(c.x+dx,c.y));
        g.strokePath(p,juce::PathStrokeType(1.5f));
    }
    g.drawLine(c.x-34,c.y,c.x-14,c.y,1);g.drawLine(c.x+14,c.y,c.x+34,c.y,1);
}
inline void knob(juce::Graphics& g,juce::Rectangle<float> bounds,double position,juce::Colour colour,bool enabled) {
    const auto c=bounds.getCentre();const float d=bounds.getWidth(),radius=d*.38f;
    const float start=juce::MathConstants<float>::pi*1.2f,span=juce::MathConstants<float>::pi*1.6f;
    auto point=[&](float angle,float r){return juce::Point<float>(c.x+std::sin(angle)*r,c.y-std::cos(angle)*r);};
    for(int i=0;i<=24;++i) {
        const auto p=point(start+span*float(i)/24,d*.48f);
        g.setColour(ink.withAlpha(enabled?.8f:.2f));g.fillEllipse(p.x-1,p.y-1,2,2);
    }
    auto disk=juce::Rectangle<float>(radius*2,radius*2).withCentre(c);
    g.setColour(juce::Colour(0x80000000));g.fillEllipse(disk.expanded(3).translated(0,3));
    g.setColour(juce::Colour(0xff03090d));g.fillEllipse(disk.expanded(2));
    if(!enabled)colour=juce::Colour(0xff4b525c);
    g.setGradientFill(juce::ColourGradient(colour.brighter(.24f),disk.getX(),disk.getY(),colour.darker(.6f),disk.getRight(),disk.getBottom(),false));g.fillEllipse(disk);
    for(int i=0;i<80;++i) {
        float a=juce::MathConstants<float>::twoPi*float(i)/80;
        g.setColour(i%2?juce::Colour(0x66000000):juce::Colour(0x26ffffff));
        g.drawLine({point(a,radius*.91f),point(a,radius*.995f)},.8f);
    }
    auto cap=disk.reduced(d*.045f);
    g.setGradientFill(juce::ColourGradient(colour.brighter(.3f),cap.getX(),cap.getY(),colour.darker(.5f),cap.getRight(),cap.getBottom(),false));g.fillEllipse(cap);
    g.setColour(colour.brighter(.6f).withAlpha(.7f));g.drawEllipse(cap.reduced(.5f),.8f);
    g.setColour(juce::Colour(0x80000000));g.drawEllipse(disk,1);
    const float angle=start+span*float(position);
    auto a=point(angle,radius*.12f),b=point(angle,radius*.94f);
    g.setColour(juce::Colour(0x55000000));g.drawLine({a.translated(1,1),b.translated(1,1)},3.8f);
    g.setColour(ink.withAlpha(enabled?1.0f:.4f));g.drawLine({a,b},2.6f);
}
inline void vu(juce::Graphics& g,juce::Rectangle<float> r,float needle) {
    g.setColour(juce::Colour(0xff02080b));g.fillRoundedRectangle(r,4);
    auto f=r.reduced(3);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffff2cb),f.getX(),f.getY(),juce::Colour(0xffd9c58a),f.getX(),f.getBottom(),false));g.fillRoundedRectangle(f,2);
    g.saveState();juce::Path clip;clip.addRoundedRectangle(f,2);g.reduceClipRegion(clip);
    const float h=f.getHeight(),radius=h*1.55f,cx=f.getCentreX(),cy=f.getY()+h*.34f+radius;
    const float span=juce::jmin(.62f,f.getWidth()*.43f/radius);
    auto angle=[&](float p){return -span+2*span*p;};
    auto pt=[&](float a,float rad){return juce::Point<float>(cx+std::sin(a)*rad,cy-std::cos(a)*rad);};
    auto pos=[](float db){return juce::jlimit(0.f,1.f,std::pow(10.f,db/20)*.7079f);};
    for(int part=0;part<2;++part){juce::Path arc;arc.addCentredArc(cx,cy,radius,radius,0,angle(part?pos(0):0),angle(part?1:pos(0)),true);g.setColour(juce::Colour(part?0xffb8322a:0xff26251e));g.strokePath(arc,juce::PathStrokeType(h*(part?.045f:.015f)));}
    for(int db : {-20,-10,-7,-5,-3,-2,-1,0,1,2,3}) {
        const auto a=angle(pos(float(db)));g.setColour(juce::Colour(db>0?0xffb8322a:0xff26251e));
        g.drawLine({pt(a,radius),pt(a,radius-h*.09f)},1);
        const auto label=pt(a,radius+h*.12f);g.setFont(juce::FontOptions(h*.095f,juce::Font::bold));
        g.drawText(juce::String(db),juce::Rectangle<float>(h*.23f,h*.16f).withCentre(label),juce::Justification::centred);
    }
    g.setColour(juce::Colour(0xff24231d));g.setFont(juce::FontOptions(h*.28f,juce::Font::bold));g.drawText("VU",f.withTrimmedTop(h*.55f).withTrimmedRight(h*.12f),juce::Justification::centredRight);
    auto a=angle(juce::jlimit(0.f,1.f,needle));g.setColour(juce::Colour(0x50000000));g.drawLine({pt(a,radius*.45f).translated(1,1),pt(a,radius*1.01f).translated(1,1)},2);
    g.setColour(juce::Colour(0xff151c1e));g.drawLine({pt(a,radius*.45f),pt(a,radius*1.01f)},1.5f);
    g.restoreState();g.setColour(rim);g.drawRoundedRectangle(r,4,.8f);
}
}
