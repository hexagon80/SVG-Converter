#include <popups.hpp>

using namespace svg;
using namespace geode::prelude;

ImportPopup* ImportPopup::create(Parser parser, Renderer renderer){
    auto ret = new ImportPopup;
    if (ret && ret->init(parser, renderer)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool ImportPopup::init(Parser parser, Renderer renderer) {
    if (!Popup::init(370.f, 300.f, "GJ_square05.png")) 
        return false;

    m_bgSprite->setColor({ 35, 15, 125 });
    m_parser = parser;
    m_renderer = renderer;
    m_parser.config.ignoreStroke = true;

    m_parser.config.quality = 1;
    m_renderer.config.quality = 1;
    m_renderer.config.Layer = 0;
    m_parser.config.ignoreStroke = false;

    auto h1 = CCLabelBMFont::create("Import SVG", "goldFont.fnt");
    h1->setScale(1.25f);
    h1->setPosition(275.f, 282.f);
    this->addChild(h1);

    auto strokeText = CCLabelBMFont::create("Ignore stroke", "bigFont.fnt");
    strokeText->setPosition({252.f, 240.f});
    strokeText->setScale(0.875f);
    this->addChild(strokeText);

    auto QualityText = CCLabelBMFont::create("Quality", "bigFont.fnt");
    QualityText->setPosition({240.f, 180.f});
    QualityText->setScale(0.975f);
    this->addChild(QualityText);

    auto LayerText = CCLabelBMFont::create("Layer", "bigFont.fnt");
    LayerText->setPosition({240.f, 85.f});
    LayerText->setScale(0.975f);
    this->addChild(LayerText);

    auto LayerInput = TextInput::create(50, "", "bigFont.fnt");
    LayerInput->setString("0");
    LayerInput->setCallbackEnabled(true);
    LayerInput->setFilter("0123456789");
    LayerInput->setPosition({330.f, 80.f});
    LayerInput->setScale(0.7f);
    this->m_layerInput = LayerInput;
    LayerInput->setCallback([this](std::string const& text) {
        onLayerInput(m_layerInput);
    });

    this->addChild(LayerInput);

    auto QualityInput = TextInput::create(50, "", "bigFont.fnt");
    QualityInput->setString("1");
    QualityInput->setCallbackEnabled(true);
    QualityInput->setFilter("0123456789");
    QualityInput->setCallback([this, QualityInput](std::string const& text) {
        onQualityInput(QualityInput);
    });
    QualityInput->setPosition({330.f, 175.f});
    QualityInput->setScale(0.7f);
    this->m_qualityInput = QualityInput;
    
    this->addChild(QualityInput);

    auto generalMenu = CCMenu::create();
    generalMenu->setPosition({ 100.f, 0.f});
    generalMenu->setContentSize({376, 306});
    generalMenu->setTouchEnabled(true);

    this->addChild(generalMenu);

    auto off = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
    auto on  = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");

    auto StrokeToggle = CCMenuItemToggler::create(
    off, on, this,
    menu_selector(ImportPopup::onStrokeToggle)
    );
    StrokeToggle->toggle(false);
    StrokeToggle->setPosition({287, 235});
    generalMenu->addChild(StrokeToggle);

    m_toggle = StrokeToggle;

    auto qualitySlider = Slider::create(
        this,
        menu_selector(ImportPopup::onSlider)
    );
    qualitySlider->setValue(0.01f);
    qualitySlider->setPosition({180.f, 150.f});
    m_slider = qualitySlider;
    generalMenu->addChild(qualitySlider);
    
    return true;
}

void ImportPopup::onStrokeToggle(CCObject*) {
    auto toggle = m_toggle;

    m_parser.config.ignoreStroke = !toggle->isToggled();
}

void ImportPopup::close() {
    onClose(nullptr);
}

void ImportPopup::onSlider(CCObject*){
    auto slider = m_slider;
    if (!slider) return;

    auto text = m_qualityInput;
    if (!text) {
        log::warn("cant find qualityInput!");
        return;
    }
    float value = slider->getValue();

    int realValue = std::clamp(static_cast<int>(value * 100.f), 1, 100);

    text->setString(fmt::format("{}", realValue));

    m_parser.config.quality = realValue;
    m_renderer.config.quality = realValue;
}

void ImportPopup::onQualityInput(CCObject* sender){
    if (sender != m_qualityInput || !m_slider) return;

    auto res = utils::numFromString<int>(m_qualityInput->getString());
    if (res.isErr()) return;
    float value = res.unwrap();

    if (value < 1) value = 1;
    if (value > 100) return;

    m_slider->setValue(value / 100.f);

    m_parser.config.quality = value;
    m_renderer.config.quality = value;
}

void ImportPopup::onLayerInput(CCObject* sender){
    if (sender != m_layerInput) return;

    auto res = utils::numFromString<short>(m_layerInput->getString());
    if (res.isErr()) return;
    short value = res.unwrap();

    m_renderer.config.Layer = value;
}

ImportPopup::CloseEvent ImportPopup::getListenForClose() {
    return this->listenForClose();
}

std::pair<Parser, Renderer> ImportPopup::getSettings(){
    return {m_parser, m_renderer};
}

ErrorPopup* ErrorPopup::create(const gd::string& str) {
    auto ret = new ErrorPopup;
    if (ret && ret->init(str)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool ErrorPopup::init(const gd::string& str) {
    if (!Popup::init(370.f, 300.f, "GJ_square05.png"))
        return false;

    m_bgSprite->setColor(ccColor3B(191, 17, 66));

    auto h1 = CCLabelBMFont::create("ERROR", "goldFont.fnt");
    h1->setScale(1.25f);
    h1->setPosition(275.f, 282.f);
    this->addChild(h1);

    auto txt = TextArea::create(str, "chatFont.fnt", 0.8f, 300.f, {0.5f, 0.5f}, 20.f, false);
    txt->setPosition({325.f, 240.f});

    this->addChild(txt);
    return true;
}

ErrorPopup::CloseEvent ErrorPopup::getListenForClose() {
    return listenForClose();
}