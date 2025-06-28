#include <Geode/Geode.hpp>
using namespace geode::prelude;

#include <regex>

auto savePathPrefix(std::string sel = "") {
    sel = sel.size() ? sel : getMod()->getSettingValue<std::string>("selected-profile");
    auto keys = getMod()->getSavedSettingsData()["profile-names"];
    auto sub = keys[sel].asString().unwrapOr("");
    return sub.size() 
        ? std::filesystem::path("geode") / "mods" / getMod()->getID() / sub / "" //custom
        : std::filesystem::path(""); //default (%writeable%/.)
}

#include <Geode/modify/GManager.hpp>
class $modify(GManagerAccountSwitcherExt, GManager) {
    void setup() {
        std::error_code err_code_to_ignore; std::filesystem::create_directories(
            CCFileUtils::get()->getWritablePath().c_str() + savePathPrefix().string(), err_code_to_ignore
        );

        m_fileName = (savePathPrefix().string() + std::string(m_fileName.c_str())).c_str();
        GManager::setup();
	}
};

//ya
#include <Geode/loader/SettingV3.hpp>

class BoolSettingNodeV3 : public SettingValueNodeV3<BoolSettingV3> {};

class StringSettingNodeV3 : public SettingValueNodeV3<StringSettingV3> {
public: TextInput* m_input;
};

class ProfileItemsSetting : public SettingBaseValueV3<matjson::Value> {
public:

    static Result<std::shared_ptr<SettingV3>> parse(std::string const& key, std::string const& modID, matjson::Value const& json) {
        auto res = std::make_shared<ProfileItemsSetting>();
        auto root = checkJson(json, "ProfileItemsSetting");
        res->parseBaseProperties(key, modID, root);
        root.checkUnknownKeys();
        return root.ok(std::static_pointer_cast<SettingV3>(res));
    }

    class Node : public SettingValueNodeV3<ProfileItemsSetting> {
    public:
        void setupList() {
            auto content = this->getParent();
            if (!content) return log::error(
                "Unable to get content layer, its {} as parent of {}"
                """""""""""""""""""""""""""", this->getParent(), this
            );

            auto oldHeight = content->getContentHeight();

            if (auto selected = content->getChildByType<StringSettingNodeV3>(0)) {
                this->runAction(CCRepeatForever::create(CCSequence::create(CallFuncExt::create(
                    [selected = Ref(selected)]() {
                        auto val = selected->getSetting()->getMod()->getSettingValue<std::string>(
                            selected->getSetting()->getKey()
                        );
                        if (val == selected->getValue()) return;
                        selected->setValue(val, selected);
                        selected->m_input->setString(selected->getValue());
                        selected->commit();
                    }
                ), nullptr)));
            };

            while (auto item = content->getChildByType<BoolSettingNodeV3>(0)) {
                item->removeFromParent();
            }

            auto keys = this->getValue();
            for (auto& key_value : keys) {
                auto key = key_value.getKey().value_or("");
                auto val = key_value.asString().unwrapOr("");
                auto aw = matjson::Value();
                aw["type"] = "bool";
                aw["name"] = key;
                aw["description"] = fmt::format(
                    "# Save folder of this profile goes at:\n"
                    "`{}`\n"
                    "[Open folder...]({})",
                    (CCFileUtils::get()->getWritablePath().c_str() / savePathPrefix(key)).string(),
                    string::replace(
                        "file://" 
                        + (CCFileUtils::get()->getWritablePath().c_str() / savePathPrefix(key)).string()
                        , "\\", "/"
                    )
                );
                aw["default"] = false;
                auto setting = BoolSettingV3::parse("dummy", GEODE_MOD_ID, aw);
                if (setting) {
                    auto node = setting.unwrapOrDefault()->createNode(this->getContentWidth());
                    content->addChild(node, -1);
                    //bg
                    node->setDefaultBGColor(ccc4(0, 0, 0, content->getChildrenCount() % 2 == 0 ? 60 : 20));
                    //deleteBtn
                    auto deleteBtn = node->getButtonMenu()->getChildByType<CCMenuItemToggler>(0);
                    auto icon = CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png");
                    limitNodeSize(icon, (deleteBtn->m_onButton->getContentSize() - CCSizeMake(14, 14)), 12.f, .1f);
                    deleteBtn->m_offButton->addChildAtPosition(icon, Anchor::Center, {}, false);
                    deleteBtn->m_onButton->addChildAtPosition(icon, Anchor::Center, {}, false);
                    CCMenuItemExt::assignCallback<CCMenuItemToggler>(
                        deleteBtn, [__this = Ref(this), key](CCMenuItemToggler* sender) {
                            sender->toggle(0);
                            auto list = __this->getValue();
                            list.erase(key);
                            __this->setValue(list, __this);
                            auto popup = createQuickPopup(
                                "Delete Save Data?",
                                "You removed profile but not save data.\n"
                                "Do you want to delete save directory of this profile too?",
                                "Persist", "Delete",
                                [key](CCNode*, bool to_delete) {
                                    if (not to_delete) return;
                                    std::error_code err_code_to_ignore;
                                    std::filesystem::remove_all(
                                        CCFileUtils::get()->getWritablePath().c_str() / savePathPrefix(key), 
                                        err_code_to_ignore
                                    );
                                }
                            );
                            if (CCKeyboardDispatcher::get()->getControlKeyPressed()) popup->onBtn2(popup);
                        }
                    );
                    //selectBtn
                    auto selectBtn = CCMenuItemExt::createTogglerWithFrameName(
                        "GJ_selectSongOnBtn_001.png", "GJ_selectSongBtn_001.png", 0.425f,
                        [__this = Ref(this), key](CCMenuItemToggler* sender) {
                            __this->getSetting()->getMod()->setSettingValue<std::string>("selected-profile", key);
                        }
                    );
                    node->runAction(CCRepeatForever::create(CCSequence::create(CallFuncExt::create(
                        [__this = Ref(this), selectBtn = Ref(selectBtn), key]() {
							auto sel = __this->getSetting()->getMod()->getSettingValue<std::string>("selected-profile");
                            selectBtn->toggle(sel == key);
						}
                    ), CCDelayTime::create(0.01f), nullptr)));
                    deleteBtn->getParent()->addChildAtPosition(selectBtn, Anchor::Left, { -10.f, 0.f });
                }
                else log::error("Unable to create setting node: {}", setting.err());
            }
            if (auto layout = typeinfo_cast<ColumnLayout*>(content->getLayout())) {
                layout->setAxisReverse(false);
            };
            this->setZOrder(999);//to be top always
            content->updateLayout();

            if (auto scroll = typeinfo_cast<CCScrollLayerExt*>(content->getParent())) {
                scroll->scrollLayer(oldHeight - content->getContentHeight());
            }
        }
        bool init(std::shared_ptr<ProfileItemsSetting> setting, float width) {
            if (!SettingValueNodeV3::init(setting, width)) return false;
            this->setContentHeight(18.f);

            this->getButtonMenu()->addChildAtPosition(CCMenuItemExt::createSpriteExtraWithFrameName(
                "GJ_plus3Btn_001.png", 0.65f, [__this = Ref(this)](CCMenuItem* sender) {
                    class InputPopup : public Popup<> {
					public:
                        Ref<SettingValueNodeV3> m_val;
                        void onClose(CCObject* sender) override {
                            //get name
                            auto item = std::string();
                            if (auto a = this->m_buttonMenu->getChildByType<TextInput>(0)) {
                                item = a->getString().c_str();
                            } else return;

                            //addit
                            auto list = m_val->getValue();
                            list[item] = std::regex_replace(item, std::regex("[:;<>\\?\"/\\\\|*]"), "_");
                            m_val->setValue(list, m_val);

                            Popup::onClose(sender);
                        }
                        bool setup() override {

                            this->setTitle("Add profile name", "geode.loader/mdFontB.fnt");

                            this->m_bgSprite->initWithFile("GJ_square07.png");

                            auto contBgSprite = CCScale9Sprite::create("geode.loader/black-square.png");
                            contBgSprite->setContentSize(m_size - 6.f);
                            contBgSprite->setPosition(this->m_bgSprite->getPosition());
                            this->m_mainLayer->addChild(contBgSprite, -1);

                            this->m_mainLayer->updateLayout();

                            auto input = TextInput::create(210, "Profile Name", "geode.loader/mdFontMono.fnt");
                            this->m_buttonMenu->addChildAtPosition(input, Anchor::Center);

                            this->m_closeBtn->removeFromParentAndCleanup(false);

                            auto apply_btnline = MDTextArea::create(
                                "[APPLY](file://)", { input->getContentSize().width - 1.5f, 20 }
                            );

                            //add action to link in
                            findFirstChildRecursive<CCMenuItem>(
                                apply_btnline, [closeBtn = Ref(this->m_closeBtn)](CCMenuItem* item) {

                                    CCMenuItemExt::assignCallback<CCMenuItem>(
                                        item, [closeBtn](CCNode*) { closeBtn->activate(); }
									);
                                    item->setPositionX(item->getParent()->getContentWidth() / 2);
                                    item->setAnchorPoint(CCPointMake(0.5f, 0.7f));

                                    return true;
                                }
                            );

                            this->m_buttonMenu->addChildAtPosition(apply_btnline, Anchor::Bottom, { 0, 18.000 });

                            return true;
						}
                        static InputPopup* create(Ref<SettingValueNodeV3> val) {
                            auto ret = new InputPopup();
                            ret->m_val = val;
                            if (ret->initAnchored(242.000f, 96.000f)) {
                                ret->autorelease();
                                return ret;
                            }
                            delete ret;
                            return nullptr;
                        }
                    };
                    InputPopup::create(__this.data())->show();
                }
            ), Anchor::Right, ccp(-6.000, 0));

            return true;
        }
        void updateState(CCNode* invoker) override {
            //log::debug("{}->updateState({})", this, invoker);
            SettingValueNodeV3::updateState(invoker);
            getSetting()->getMod()->saveData();
            getBG()->setOpacity(106);
            setupList();
        }
        static Node* create(std::shared_ptr<ProfileItemsSetting> setting, float width) {
            auto ret = new Node();
            if (ret && ret->init(setting, width)) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }
    };

    SettingNodeV3* createNode(float width) override {
        return Node::create(std::static_pointer_cast<ProfileItemsSetting>(shared_from_this()), width);
    };

};

void modLoaded() {
    auto ret = Mod::get()->registerCustomSettingType("profile-names", &ProfileItemsSetting::parse);
    if (!ret) log::error("Unable to register setting type: {}", ret.unwrapErr());
}
$on_mod(Loaded) { modLoaded(); }