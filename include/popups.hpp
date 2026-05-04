#pragma once

#include <Geode/Geode.hpp>
#include <svg.hpp>

class ImportPopup : public geode::Popup {
public:
    static ImportPopup* create(svg::Parser parser, svg::Renderer renderer);

    std::pair<svg::Parser, svg::Renderer> getSettings(); 

    CloseEvent getListenForClose();

    void close();

protected:
    svg::Parser m_parser;
    svg::Renderer m_renderer;

    CCMenuItemToggler* m_toggle;
    geode::TextInput* m_layerInput;
    geode::TextInput* m_qualityInput;
    Slider* m_slider;

    bool init(svg::Parser parser, svg::Renderer renderer);

    void onStrokeToggle(CCObject*);
    void onSlider(CCObject*);
    void onQualityInput(CCObject*);
    void onLayerInput(CCObject*);
};
