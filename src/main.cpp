#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <svg.hpp>
#include <popups.hpp>
#include <errors.hpp>

using namespace geode::prelude;
using namespace svg;

class $modify(MyEditorHook, LevelEditorLayer) {
public:

    struct Fields {
        ListenerHandle importPopupListener;
        ListenerHandle errorPopupListener;
        
        bool isPopUpActive = false;
        bool isPickerActive = false;
        size_t placeIndex = 0;

        // When creating a bunch of colors, getNextColorID give wrong IDs,
        // so we initialize with getNextColorID and then we follow our own count
        int nextColorID = -1;

        Parser parser;
        Renderer renderer;

        CCArray* ObjsPlaced = nullptr;

        std::vector<ObjCommand> ObjsToPlace;
        std::unordered_map<uint32_t, int> resolvedColorIDs;
    };

    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;

        listenForKeybindSettingPresses("import", [this](Keybind const& keybind, bool down, bool repeat, double timestamp) {
            if (down && !repeat && !(m_fields->isPopUpActive || m_fields->isPickerActive)) {
                onPickFile(nullptr);
            }
        });
        return true;
    }

    void onPickFile(CCObject*){
        if (m_fields->isPickerActive || m_fields->isPopUpActive) return;

        m_fields->isPickerActive = true;

        file::FilePickOptions options = { 
            std::nullopt,
            {  { "SVG Files", { "*.svg", ".SVG" }} }
        };

        auto self = WeakRef(this);

        async::spawn(
            file::pick(
                file::PickMode::OpenFile,
                options
        ),
        [self](Result<std::optional<std::filesystem::path>> result) {
            auto layer = self.lock();
            if (!layer) return;
            if (result.isOk()) {
                auto opt = result.unwrap();
                if (opt) {

                    auto file = opt.value();

                    layer->m_fields->isPickerActive = false;
                    
                    layer->onImportPopup(file);
                } else {
                    // User cancelled the dialog
                    log::info("User cancelled!");
                    layer->m_fields->isPickerActive = false;
                }
            }
            else {
                log::error("fatal error: unable to pick file!");
                layer->m_fields->isPickerActive = false;
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

        if (!popup) {
            m_fields->isPopUpActive = false;
            return;
        }

        popup->setID("import-popup");

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

    void onErrorPopup(gd::string text) {
        CleanFields();
        auto popup = ErrorPopup::create(text);

        if (!popup) return;

        popup->m_noElasticity = true;

        popup->show();

        m_fields->isPopUpActive = true;

        m_fields->errorPopupListener = popup->getListenForClose().listen([this]() {
            m_fields->isPopUpActive = false;
            return ListenerResult::Propagate;
        });
    }

    void Import(CCObject* sender){
        log::info ("importing svg...");

        m_fields->isPopUpActive = false;

        auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);

        // Protected
        auto popup = typeinfo_cast<ImportPopup*>(btn->getParent()->getParent());
        if (!popup) {
            log::warn ("error casting!"); return;
        }
        
        Parser parser = popup->getSettings().first;
        Renderer renderer = popup->getSettings().second; 

        popup->close();

        renderer.config.position = getCurrentPos();

        auto self = WeakRef(this);

        // Blocking task for the high-CPU consuptive Parser & Triangulation.
        async::spawn(
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
            [self](geode::Result<RenderResult> res) {
                auto layer = self.lock();
                if (!layer) return;

                if (res.isErr()) {
                    layer->onErrorPopup(res.unwrapErr());
                    
                };
                auto& render = res.unwrap();

                auto& commands = render.commands;
                auto& usedColors = render.usedColors;

                if (commands.empty()) {
                    layer->onErrorPopup("Failed to parse the SVG file. Please check the file format.");
                    return;
                }

                if (render.commands.size() > 50000) {
                    layer->onErrorPopup(ERR_TOO_MUCH_OBJECTS);
                    return;
                };

                if (render.usedColors.size() + layer->getNextColorChannel() > 999) {
                    layer->onErrorPopup(ERR_COLOR_LIMIT_REACHED);
                    return;
                }

                layer->m_fields->resolvedColorIDs.clear();
                layer->m_fields->resolvedColorIDs.reserve(usedColors.size());
                layer->m_fields->nextColorID = layer->getNextColorChannel();

                for (auto key : usedColors) {
                    auto color = UnpackColor(key);

                    int id = layer->newColor(ccc3(color.r, color.g, color.b));

                    layer->m_fields->resolvedColorIDs[key] = id;
                }

                // A copy would be titanic, up to 50k commands!
                layer->m_fields->ObjsToPlace = std::move(render.commands);
                layer->m_fields->placeIndex = 0;
                layer->m_fields->ObjsPlaced = CCArray::create();
                layer->m_fields->ObjsPlaced->retain();
                layer->schedule(schedule_selector(MyEditorHook::PlaceObjects));
            }
        );
    }

    CCPoint getCurrentPos(){
        auto main = this->getChildByID("main-node");
        if (!main) {
            log::warn("main-node not found!");
            return {0,0};
        }
        auto node = main->getChildByID("batch-layer");
        if (!node) {
            log::warn("batch-layer not found!");
            return {0,0};
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        CCPoint center = {winSize.width / 2, winSize.height / 2};

        CCPoint worldPos = node->convertToNodeSpace(center);

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

    // Places a chunk of 200 objects per tick
    void PlaceObjects(float dt) {
        constexpr int PER_TICK = 200;
        const auto& commands = m_fields->ObjsToPlace;

        for (int i = 0; i < PER_TICK && m_fields->placeIndex < commands.size(); i++) {
            auto& command = commands[m_fields->placeIndex++];

            // Prevent scale reset on all objects
            if (command.scaleX < 0.001f || command.scaleY < 0.001f) continue;

            auto obj = this->createObject(command.key, command.pos, true);
            if (!obj) {
                // This one might be caused beacuse of limit object reached
                this->unschedule(schedule_selector(MyEditorHook::PlaceObjects));
                onErrorPopup(ERR_UNKNOW);
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
            obj->m_editorLayer = command.layer;

            if (m_undoObjects && m_undoObjects->count() > 0) {
                m_undoObjects->removeLastObject();
            }

            m_fields->ObjsPlaced->addObject(obj);
        }

        if (m_fields->placeIndex >= commands.size()) {
            // continue on next tick
            this->unschedule(schedule_selector(MyEditorHook::PlaceObjects));
            groupStickyObjects(m_fields->ObjsPlaced);
            CleanFields();
            log::info("finished!");
        }
    }

    void CleanFields() {
        if (m_fields->ObjsPlaced) {
            m_fields->ObjsPlaced->release();
            m_fields->ObjsPlaced = nullptr;
        }
        m_fields->ObjsToPlace.clear();
        m_fields->parser = Parser{};
        m_fields->renderer = Renderer{};
        m_fields->resolvedColorIDs.clear();
        m_fields->placeIndex = 0;
        m_fields->nextColorID = -1;
    }
};
