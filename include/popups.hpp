#pragma once

#include <Geode/Geode.hpp>
#include <svg.hpp>

using namespace geode::prelude;

class ImportPopup : public Popup {
public:
    static ImportPopup* create(svg::Parser parser, svg::Renderer renderer);

    std::pair<svg::Parser, svg::Renderer> getSettings(); 

    CloseEvent getListenForClose();

    void close();

protected:
    svg::Parser m_parser;
    svg::Renderer m_renderer;

    CCMenuItemToggler* m_toggle;
    TextInput* m_layerInput;
    TextInput* m_qualityInput;
    Slider* m_slider;

    bool init(svg::Parser parser, svg::Renderer renderer);

    void onStrokeToggle(CCObject*);
    void onSlider(CCObject*);
    void onQualityInput(CCObject*);
    void onLayerInput(CCObject*);
};

class ErrorPopup : public Popup {
public:
    static ErrorPopup* create(const gd::string& str);
    
    CloseEvent getListenForClose();

private:
    gd::string m_text = "";

    bool init(const gd::string& str);
};