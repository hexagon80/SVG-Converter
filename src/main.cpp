#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <svg.hpp>
#include <popups.hpp>

// TODO: Add drag & drop api implementation

using namespace geode::prelude;
using namespace svg;

class $modify(MyEditorHook, LevelEditorLayer) {
public:
    struct Fields {
        ListenerHandle importPopupListener;
        ListenerHandle errorPopupListener;

        async::TaskHolder<geode::Result<RenderResult>> asyncListener; // importing async

        bool isPopUpActive = false;
        bool isPickerActive = false;
        size_t placeIndex = 0;
        short placeLayer = 0;

        // When creating a bunch of colors, getNextColorID give wrong IDs,
        // so we initialize with getNextColorID and then we follow our own count
        int nextColorID = -1;

        CCArray* ObjsPlaced = nullptr;

        std::vector<ObjCommand> ObjsToPlace;
        std::unordered_map<uint32_t, int> resolvedColorIDs;

        ~Fields() {
            if (ObjsPlaced) {
                ObjsPlaced->release();
                ObjsPlaced = nullptr;
            }
            if (!ObjsToPlace.empty()) ObjsToPlace.clear();
            if (!resolvedColorIDs.empty()) resolvedColorIDs.clear();
            asyncListener.cancel();
        }
    };

    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;

        this->addEventListener(
            KeybindSettingPressedEventV3(Mod::get(), "import"),
            [this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
                if (LevelEditorLayer::get() != this) return;
                if (down && !repeat) {
                    onPickFile();
                }
            }
        );
        return true;
    }

    void onPickFile(){
        if (m_fields->isPickerActive || m_fields->isPopUpActive) return;

        m_fields->isPickerActive = true;

        file::FilePickOptions options = { 
            std::nullopt,
            {  { "SVG Files", { "*.svg", ".SVG" }} }
        };

        async::spawn(
            file::pick(
                file::PickMode::OpenFile,
                options
        ),
        [this](Result<std::optional<std::filesystem::path>> result) {
            m_fields->isPickerActive = false;
            if (result.isOk()) {
                auto opt = result.unwrap();
                if (opt) {

                    auto file = opt.value();

                    onImportPopup(file);
                } else {
                    // User cancelled the dialog
                    log::info("User cancelled!");
                    return;
                }
            }
            else {
                log::error("fatal error: unable to pick file!");
                return;
            }
        });
    }

    void onImportPopup(std::filesystem::path file){
        if (m_fields->isPopUpActive) return;

        Parser p;
        Renderer r;

        p.file = file;

        auto popup = ImportPopup::create(p, r);

        if (!popup) return;

        popup->setID("import-popup"_spr);

        popup->m_noElasticity = true;

        auto BtnSpr = ButtonSprite::create("Import"); 

        auto Btn = CCMenuItemSpriteExtra::create(
            BtnSpr,
            this,
            menu_selector(MyEditorHook::Import)
        );

        Btn->setPosition(63, 18.f);

        auto menu = CCMenu::create();

        menu->addChild(Btn);

        menu->setPosition(221.f, 19.f);
        menu->setContentSize({127.f, 36.f});

        popup->addChild(menu);

        popup->show();

        m_fields->isPopUpActive = true;

        m_fields->importPopupListener  = popup->getListenForClose().listen([this]() {
            m_fields->isPopUpActive = false;
            return ListenerResult::Propagate;
        });
    }

    void onError(gd::string text) {
        CleanFields();
        auto notification = Notification::create(text, NotificationIcon::Error, 1.f);
        if (!notification) return;

        notification->show();

        log::warn("{}", text);
    }

    void Import(CCObject* sender){
        log::info ("importing svg...");

        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);

        auto popup = typeinfo_cast<ImportPopup*>(btn->getParent()->getParent());
        if (!popup) {
            log::warn ("error casting!"); 
            return;
        }

        Parser parser = popup->getSettings().first;
        Renderer renderer = popup->getSettings().second;

        m_fields->placeLayer = renderer.config.Layer;

        popup->close();

        m_fields->isPopUpActive = false;

        renderer.config.position = getCurrentPos();

        // Blocking task for the high-CPU consuptive Parser & Triangulation.
        m_fields->asyncListener.spawn(
            arc::spawnBlocking<geode::Result<RenderResult>>(
                [parser, renderer]() mutable ->  geode::Result<RenderResult> 
                {
                    auto res = parser.Parse();
                    if (res.isErr())
                        return Err(gd::string(res.unwrapErr()));

                    renderer.svg = res.unwrap();

                    return Ok(renderer.RenderSVG());
                }
            ),
            [this](geode::Result<RenderResult> res) {
                if (LevelEditorLayer::get() != this) return;

                if (res.isErr()) {
                    onError(res.unwrapErr());
                    return;
                };
                auto& render = res.unwrap();

                auto& commands = render.commands;
                auto& usedColors = render.usedColors;

                if (commands.empty()) {
                    onError("Failed to parse the SVG!");
                    return;
                }

                if (render.commands.size() > 50000) {
                    onError("Error - Too much objects!");
                    return;
                };

                if (usedColors.size() + getNextColorChannel() > 999) {
                    onError("Error - Not enough colors!");
                    return;
                }

                m_fields->resolvedColorIDs.clear();
                m_fields->resolvedColorIDs.reserve(usedColors.size());
                m_fields->nextColorID = getNextColorChannel();

                for (auto key : usedColors) {
                    auto color = UnpackColor(key);

                    int id = newColor(ccc3(color.r, color.g, color.b));

                    m_fields->resolvedColorIDs[key] = id;
                }

                m_fields->ObjsToPlace = std::move(render.commands);
                m_fields->placeIndex = 0;
                m_fields->ObjsPlaced = CCArray::create();
                m_fields->ObjsPlaced->retain();

                auto bar = ProgressBar::create(ProgressBarStyle::Slider);
                bar->setZOrder(101);
                bar->setPosition({280.f, 120.f});
                bar->setFillColor({0, 140, 255});
                bar->setID("progress-bar"_spr);
                bar->setAnchorPoint({0.5f, 0.5f});
                bar->updateProgress(0.0f);

                this->addChild(bar);

                schedule(schedule_selector(MyEditorHook::PlaceObjects));
            }
        );
    }

    CCPoint getCurrentPos(){
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        CCPoint center = {winSize.width / 2, winSize.height / 2};

        CCPoint worldPos = m_objectLayer->convertToNodeSpace(center);

        return worldPos;
    }

    int newColor(ccColor3B color) {
        int nextID = m_fields->nextColorID++;

        if (nextID == 0) nextID = getNextColorChannel();

        // This should never happen, we have checked it before placing!
        if (nextID >= 999) {
            log::warn("Color channel limit reached!");
            return -1;
        }

        auto newAction = ColorAction::create(color, false, 0);
        m_levelSettings->m_effectManager->setColorAction(newAction, nextID);

        return nextID;
    }

    // Places a chunk of objects per tick
    void PlaceObjects(float dt) {
        // LeL is alive while transitioning, wich lets PlaceObjects
        // continue for some ticks, and places objects but doesn't saves them, wich is risky ig.
        if (CCDirector::get()->getIsTransitioning()) {
            unschedule(schedule_selector(MyEditorHook::PlaceObjects));
            return;
        }

        const int PER_TICK = Mod::get()->getSettingValue<int>("speed");
        const auto& commands = m_fields->ObjsToPlace;

        auto bar = static_cast<ProgressBar*>(this->getChildByID("progress-bar"_spr));
        if (!bar) {
            log::warn("bar null");
            unschedule(schedule_selector(MyEditorHook::PlaceObjects));
            return;
        }

        for (int i = 0; i < PER_TICK && m_fields->placeIndex < commands.size(); i++) {
            auto& command = commands[m_fields->placeIndex++];

            // Prevent scale reset on all objects
            if (command.scaleX < 0.001f || command.scaleY < 0.001f) continue;

            auto obj = createObject(command.key, command.pos, true);

            if (!obj) {
                // This one might be caused beacuse of limit object reached
                unschedule(schedule_selector(MyEditorHook::PlaceObjects));
                bar->removeFromParent();
                onError("Error - Unknow error!");
                return;
            };

            obj->updateCustomScaleX(command.scaleX);
            obj->updateCustomScaleY(command.scaleY);
            obj->setRotationX(command.rotX);
            obj->setRotationY(command.rotY);

            auto it = m_fields->resolvedColorIDs.find(command.colorKey);
            if (it != m_fields->resolvedColorIDs.end() && obj->m_baseColor)
                obj->m_baseColor->m_colorID = it->second; 

            obj->m_zLayer = ZLayer::B1;
            obj->m_editorLayer = m_fields->placeLayer;

            if (m_undoObjects && m_undoObjects->count() > 0) {
                m_undoObjects->removeLastObject();
            }

            m_fields->ObjsPlaced->addObject(obj);
            bar->updateProgress((float)m_fields->placeIndex / commands.size() * 100.f);
        }

        if (m_fields->placeIndex >= commands.size()) {
            unschedule(schedule_selector(MyEditorHook::PlaceObjects));

            groupStickyObjects(m_fields->ObjsPlaced);

            bar->removeFromParent();

            auto notification = Notification::create("Imported!", NotificationIcon::Success, 1.f);
            if (notification) notification->show();

            CleanFields();

            log::info("finished!");
        }
    }

    void CleanFields() {
        if (m_fields->ObjsPlaced) {
            m_fields->ObjsPlaced->release();
            m_fields->ObjsPlaced = nullptr;
        }
        if (!m_fields->ObjsToPlace.empty()) m_fields->ObjsToPlace.clear();
        if (!m_fields->resolvedColorIDs.empty()) m_fields->resolvedColorIDs.clear();
        m_fields->placeLayer = 0;
        m_fields->placeIndex = 0;
        m_fields->nextColorID = -1;
    }
};
