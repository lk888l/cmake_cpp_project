#include "app/menu_application.hpp"

#include "app/menu_model.hpp"
#include "assets/fonts/font_ui_cn.hpp"

#include "lvgl.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace {

constexpr std::int32_t kScreenWidth = 240;
constexpr std::int32_t kScreenHeight = 240;
constexpr std::int32_t kNormalScale = LV_SCALE_NONE;
constexpr std::int32_t kPressedScale = 246; // 96 % of LVGL's 256 base scale.

constexpr std::uint32_t kColorBackground = 0x0B1020;
constexpr std::uint32_t kColorPanel = 0x151D31;
constexpr std::uint32_t kColorPanelFocused = 0x213352;
constexpr std::uint32_t kColorAccent = 0x58D6C7;
constexpr std::uint32_t kColorAccentWarm = 0xFFB86B;
constexpr std::uint32_t kColorText = 0xF3F6FF;
constexpr std::uint32_t kColorMuted = 0x94A3B8;
constexpr std::uint32_t kColorTrack = 0x2A3852;

constexpr std::array<const char*, MenuModel::HomeItemCount> kHomeTitles{
    "圆环调节",
    "开关互动",
    "动画实验",
    "按键状态",
};

constexpr std::array<const char*, MenuModel::HomeItemCount> kHomeHints{
    "0 - 100 / 步长 5",
    "两个动画 Switch",
    "速度 / 播放 / 轨道",
    "实时状态 / 次数",
};

constexpr std::array<const char*, 3> kButtonNames{
    "上键",
    "下键",
    "确认键",
};

lv_color_t color(std::uint32_t rgb)
{
    return lv_color_hex(rgb);
}

void setObjectX(void* object, std::int32_t value)
{
    lv_obj_set_x(static_cast<lv_obj_t*>(object), value);
}

void setObjectY(void* object, std::int32_t value)
{
    lv_obj_set_y(static_cast<lv_obj_t*>(object), value);
}

void setObjectScale(void* object, std::int32_t value)
{
    lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(object), value, 0);
}

void setObjectOpacity(void* object, std::int32_t value)
{
    lv_obj_set_style_opa(
        static_cast<lv_obj_t*>(object), static_cast<lv_opa_t>(std::clamp(value, 0, 255)), 0);
}

void setObjectTranslateY(void* object, std::int32_t value)
{
    lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(object), value, 0);
}

void setArcValue(void* object, std::int32_t value)
{
    lv_arc_set_value(static_cast<lv_obj_t*>(object), value);
}

void animateValue(void* object,
                  lv_anim_exec_xcb_t setter,
                  std::int32_t from,
                  std::int32_t to,
                  std::uint32_t duration,
                  lv_anim_path_cb_t path = lv_anim_path_ease_out)
{
    if (object == nullptr) {
        return;
    }

    lv_anim_delete(object, setter);
    if (from == to) {
        setter(object, to);
        return;
    }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, object);
    lv_anim_set_exec_cb(&animation, setter);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_duration(&animation, duration);
    lv_anim_set_path_cb(&animation, path);
    lv_anim_start(&animation);
}

lv_obj_t* createLabel(lv_obj_t* parent,
                      const char* text,
                      const lv_font_t* font,
                      std::uint32_t textColor = kColorText)
{
    auto* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(textColor), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    return label;
}

void stylePage(lv_obj_t* page)
{
    lv_obj_set_size(page, kScreenWidth, kScreenHeight);
    lv_obj_set_style_bg_color(page, color(kColorBackground), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
}

void stylePanel(lv_obj_t* panel, std::uint32_t panelColor = kColorPanel)
{
    lv_obj_set_style_bg_color(panel, color(panelColor), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, color(kColorTrack), 0);
    lv_obj_set_style_shadow_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

} // namespace

struct MenuApplication::Impl final {
    struct Telemetry final {
        bool pressed{};
        std::uint32_t pressCount{};
        std::uint32_t heldMs{};
        std::string lastEvent{"-"};
    };

    lv_obj_t* root{};
    lv_obj_t* homePage{};
    lv_obj_t* detailPage{};
    lv_group_t* group{};
    lv_indev_t* indev{};

    std::array<lv_obj_t*, MenuModel::HomeItemCount> cards{};
    std::array<lv_obj_t*, MenuModel::HomeItemCount> cardTitles{};
    std::array<lv_obj_t*, MenuModel::HomeItemCount> cardHints{};
    lv_obj_t* homeCounter{};
    lv_obj_t* homeMode{};

    lv_obj_t* detailTitle{};
    lv_obj_t* detailMode{};
    std::vector<lv_obj_t*> detailFocusTargets;
    lv_obj_t* arc{};
    lv_obj_t* arcValue{};
    std::array<lv_obj_t*, 2> switches{};
    lv_obj_t* speedRoller{};
    lv_obj_t* playLabel{};
    lv_obj_t* orbitTrack{};
    lv_obj_t* orbitDot{};
    std::array<lv_obj_t*, 3> buttonDots{};
    std::array<lv_obj_t*, 3> buttonStateLabels{};
    std::array<lv_obj_t*, 3> buttonDetailLabels{};
    std::array<lv_obj_t*, 3> buttonEventLabels{};

    MenuModel model;
    MenuSnapshot rendered{};
    runtime::RenderPolicy policy{};
    std::uint32_t elapsedMs{};
    std::uint32_t animationAccumulatorMs{};
    bool created{};
    bool largeObjectLayersEnabled{};
    lv_obj_t* pressedTarget{};
    std::int32_t pressedBaseX{};
    std::int16_t pendingEncoderDiff{};
    std::uint32_t pendingKey{};
    lv_indev_state_t pendingState{LV_INDEV_STATE_RELEASED};

    std::mutex telemetryMutex;
    std::array<Telemetry, 3> telemetry{};
    bool telemetryDirty{};

    ~Impl()
    {
        if (indev != nullptr) {
            lv_indev_delete(indev);
            indev = nullptr;
        }
        if (group != nullptr) {
            lv_group_delete(group);
            group = nullptr;
        }
        if (homePage != nullptr && lv_obj_is_valid(homePage)) {
            lv_obj_delete(homePage);
            homePage = nullptr;
        }
        if (detailPage != nullptr && lv_obj_is_valid(detailPage)) {
            lv_obj_delete(detailPage);
            detailPage = nullptr;
        }
    }

    static std::size_t buttonIndex(input::PhysicalButton button)
    {
        switch (button) {
        case input::PhysicalButton::Up: return 0;
        case input::PhysicalButton::Down: return 1;
        case input::PhysicalButton::Confirm: return 2;
        }
        return 0;
    }

    static void readInput(lv_indev_t* indevHandle, lv_indev_data_t* data)
    {
        auto* self = static_cast<Impl*>(lv_indev_get_user_data(indevHandle));
        data->enc_diff = std::exchange(self->pendingEncoderDiff, 0);
        data->key = self->pendingKey;
        data->state = self->pendingState;
        data->continue_reading = false;
    }

    void sendEncoder(std::int16_t diff)
    {
        pendingEncoderDiff = diff;
        pendingKey = 0;
        pendingState = LV_INDEV_STATE_RELEASED;
        lv_indev_read(indev);
    }

    void sendEnterClick()
    {
        pendingEncoderDiff = 0;
        pendingKey = LV_KEY_ENTER;
        pendingState = LV_INDEV_STATE_PRESSED;
        lv_indev_read(indev);
        pendingState = LV_INDEV_STATE_RELEASED;
        lv_indev_read(indev);
        pendingKey = 0;
    }

    bool create(lv_obj_t* targetRoot, const runtime::RenderPolicy& renderPolicy)
    {
        if (targetRoot == nullptr || created) {
            return false;
        }

        root = targetRoot;
        policy = renderPolicy;
        lv_obj_set_style_bg_color(root, color(kColorBackground), 0);
        lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
        lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

        homePage = lv_obj_create(root);
        detailPage = lv_obj_create(root);
        stylePage(homePage);
        stylePage(detailPage);
        lv_obj_set_pos(homePage, 0, 0);
        lv_obj_set_pos(detailPage, kScreenWidth, 0);

        createHome();

        group = lv_group_create();
        if (group == nullptr) {
            return false;
        }
        lv_group_set_wrap(group, true);

        indev = lv_indev_create();
        if (indev == nullptr) {
            return false;
        }
        lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
        lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);
        lv_indev_set_read_cb(indev, readInput);
        lv_indev_set_user_data(indev, this);
        lv_indev_set_group(indev, group);

        if (policy.allowLargeObjectLayers) {
            lv_mem_monitor_t memory{};
            lv_mem_monitor(&memory);
            largeObjectLayersEnabled = runtime::hasLargeLayerBudget(
                policy, memory.free_biggest_size, 208, 112);
            if (!largeObjectLayersEnabled) {
                std::fprintf(stderr,
                             "warning: quality layer effects disabled: largest LVGL block "
                             "%zu bytes, need %zu bytes\n",
                             memory.free_biggest_size,
                             runtime::estimateLargeLayerBytes(208, 112, policy.bufferLines));
            }
        }

        rendered = model.snapshot();
        layoutHome(false);
        rebuildGroup();
        created = true;
        return true;
    }

    void createHome()
    {
        for (std::size_t index = 0; index < cards.size(); ++index) {
            auto* card = lv_obj_create(homePage);
            cards[index] = card;
            lv_obj_set_size(card, 208, 112);
            stylePanel(card);
            lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

            auto* marker = lv_obj_create(card);
            lv_obj_set_size(marker, 5, 48);
            lv_obj_set_pos(marker, 13, 22);
            lv_obj_set_style_bg_color(marker, color(index % 2 == 0 ? kColorAccent : kColorAccentWarm), 0);
            lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(marker, 0, 0);
            lv_obj_set_style_radius(marker, 3, 0);
            lv_obj_remove_flag(marker, LV_OBJ_FLAG_SCROLLABLE);

            cardTitles[index] = createLabel(card, kHomeTitles[index], &lv_font_ui_cn_20);
            lv_obj_set_pos(cardTitles[index], 32, 23);
            cardHints[index] = createLabel(card, kHomeHints[index], &lv_font_ui_cn_16, kColorMuted);
            lv_obj_set_pos(cardHints[index], 32, 59);
        }

        auto* header = lv_obj_create(homePage);
        lv_obj_set_size(header, kScreenWidth, 45);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, color(kColorBackground), 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
        createLabel(header, "交互菜单", &lv_font_ui_cn_20);
        auto* title = lv_obj_get_child(header, 0);
        lv_obj_set_pos(title, 14, 9);

        homeMode = createLabel(header, "浏览", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(homeMode, LV_ALIGN_RIGHT_MID, -14, 0);

        auto* footer = lv_obj_create(homePage);
        lv_obj_set_size(footer, kScreenWidth, 36);
        lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(footer, color(kColorBackground), 0);
        lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(footer, 0, 0);
        lv_obj_set_style_pad_all(footer, 0, 0);
        lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

        auto* help = createLabel(footer, "上/下 选择   确认 进入", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(help, LV_ALIGN_LEFT_MID, 13, 0);
        homeCounter = createLabel(footer, "1/4", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(homeCounter, LV_ALIGN_RIGHT_MID, -13, 0);
    }

    void layoutHome(bool animated)
    {
        const auto selected = model.snapshot().homeSelection;
        for (std::size_t index = 0; index < cards.size(); ++index) {
            const auto relative = static_cast<std::uint8_t>(
                (index + MenuModel::HomeItemCount - selected) % MenuModel::HomeItemCount);

            std::int32_t x = 16;
            std::int32_t y = 62;
            std::int32_t scale = kNormalScale;
            std::int32_t opacity = LV_OPA_COVER;
            if (relative == 1) {
                x = 28;
                y = 188;
                scale = 232;
                opacity = 110;
            }
            else if (relative == MenuModel::HomeItemCount - 1) {
                x = 28;
                y = -51;
                scale = 232;
                opacity = 110;
            }
            auto* card = cards[index];
            if (relative == 2) {
                lv_anim_delete(card, setObjectX);
                lv_anim_delete(card, setObjectY);
                lv_anim_delete(card, setObjectScale);
                lv_anim_delete(card, setObjectOpacity);
                setObjectScale(card, kNormalScale);
                setObjectOpacity(card, LV_OPA_COVER);
                lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);

            if (!largeObjectLayersEnabled) {
                scale = kNormalScale;
                opacity = LV_OPA_COVER;
            }
            const bool highlight = model.snapshot().switches[1];
            lv_obj_set_style_border_width(card, relative == 0 ? 2 : 1, 0);
            lv_obj_set_style_border_color(
                card,
                color(relative == 0
                          ? (highlight ? kColorAccent : kColorMuted)
                          : kColorTrack),
                0);
            lv_obj_set_style_bg_color(
                card,
                color(relative == 0 && highlight ? kColorPanelFocused : kColorPanel),
                0);
            lv_obj_set_style_text_color(
                cardTitles[index], color(relative == 0 ? kColorText : kColorMuted), 0);
            lv_obj_set_style_text_color(cardHints[index], color(kColorMuted), 0);
            if (animated) {
                animateValue(card, setObjectX, lv_obj_get_x(card), x, MotionTiming::FocusMs);
                animateValue(card, setObjectY, lv_obj_get_y(card), y, MotionTiming::FocusMs);
                if (largeObjectLayersEnabled) {
                    animateValue(card,
                                 setObjectScale,
                                 lv_obj_get_style_transform_scale_x_safe(card, LV_PART_MAIN),
                                 scale,
                                 MotionTiming::FocusMs);
                    animateValue(card,
                                 setObjectOpacity,
                                 lv_obj_get_style_opa(card, 0),
                                 opacity,
                                 MotionTiming::FocusMs);
                }
                else {
                    setObjectScale(card, kNormalScale);
                    setObjectOpacity(card, LV_OPA_COVER);
                }
            }
            else {
                lv_obj_set_pos(card, x, y);
                setObjectScale(card, scale);
                setObjectOpacity(card, opacity);
            }
        }

        char counter[8]{};
        std::snprintf(counter, sizeof(counter), "%u/4", static_cast<unsigned>(selected + 1));
        lv_label_set_text(homeCounter, counter);
    }

    lv_obj_t* createDetailRow(std::int32_t y, std::int32_t height)
    {
        auto* row = lv_obj_create(detailPage);
        lv_obj_set_size(row, 220, height);
        lv_obj_set_pos(row, 10, y);
        stylePanel(row);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        detailFocusTargets.push_back(row);
        return row;
    }

    void createDetailHeader(const char* title)
    {
        detailTitle = createLabel(detailPage, title, &lv_font_ui_cn_20);
        lv_obj_set_pos(detailTitle, 13, 8);
        detailMode = createLabel(detailPage, "浏览", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(detailMode, LV_ALIGN_TOP_RIGHT, -13, 10);
        auto* help = createLabel(detailPage, "长按确认 返回", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(help, LV_ALIGN_BOTTOM_MID, 0, -7);
    }

    void buildArcPage()
    {
        createDetailHeader("圆环调节");
        auto* panel = createDetailRow(42, 166);
        arc = lv_arc_create(panel);
        lv_obj_set_size(arc, 128, 128);
        lv_obj_align(arc, LV_ALIGN_CENTER, 0, -5);
        lv_arc_set_range(arc, 0, 100);
        lv_arc_set_bg_angles(arc, 135, 45);
        lv_arc_set_rotation(arc, 0);
        lv_arc_set_value(arc, model.snapshot().arcValue);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_color(arc, color(kColorTrack), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, color(kColorAccent), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(arc, color(kColorAccentWarm), LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc, 3, LV_PART_KNOB);

        char initialValue[8]{};
        std::snprintf(initialValue,
                      sizeof(initialValue),
                      "%u",
                      static_cast<unsigned>(model.snapshot().arcValue));
        arcValue = createLabel(panel, initialValue, &lv_font_ui_cn_20);
        lv_obj_align(arcValue, LV_ALIGN_CENTER, 0, -10);
        auto* unit = createLabel(panel, "数值 / 100", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(unit, LV_ALIGN_CENTER, 0, 18);
        auto* hint = createLabel(panel, "确认 编辑 / 保存", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
    }

    void buildSwitchPage()
    {
        createDetailHeader("开关互动");
        constexpr std::array<const char*, 2> labels{"呼吸动画", "高亮反馈"};
        for (std::size_t index = 0; index < switches.size(); ++index) {
            auto* row = createDetailRow(51 + static_cast<std::int32_t>(index) * 70, 56);
            auto* title = createLabel(row, labels[index], &lv_font_ui_cn_16);
            lv_obj_align(title, LV_ALIGN_LEFT_MID, 14, 0);

            auto* toggle = lv_switch_create(row);
            switches[index] = toggle;
            lv_obj_set_size(toggle, 48, 26);
            lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -14, 0);
            lv_obj_remove_flag(toggle, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(toggle, color(kColorTrack), LV_PART_MAIN);
            lv_obj_set_style_bg_color(
                toggle,
                color(kColorAccent),
                static_cast<lv_style_selector_t>(
                    static_cast<std::uint32_t>(LV_PART_INDICATOR)
                    | static_cast<std::uint32_t>(LV_STATE_CHECKED)));
            lv_obj_set_style_bg_color(toggle, color(kColorText), LV_PART_KNOB);
        }
        auto* hint = createLabel(detailPage, "确认 切换", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -31);
    }

    void buildAnimationPage()
    {
        createDetailHeader("动画实验");
        auto* speedRow = createDetailRow(43, 72);
        auto* speedTitle = createLabel(speedRow, "速度", &lv_font_ui_cn_16);
        lv_obj_align(speedTitle, LV_ALIGN_LEFT_MID, 15, 0);
        speedRoller = lv_roller_create(speedRow);
        lv_roller_set_options(speedRoller, "慢\n中\n快", LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(speedRoller, 1);
        lv_obj_set_size(speedRoller, 76, 52);
        lv_obj_align(speedRoller, LV_ALIGN_RIGHT_MID, -13, 0);
        lv_obj_remove_flag(speedRoller, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_text_font(speedRoller, &lv_font_ui_cn_16, 0);
        lv_obj_set_style_text_font(speedRoller, &lv_font_ui_cn_16, LV_PART_SELECTED);
        lv_obj_set_style_bg_color(speedRoller, color(kColorPanel), 0);
        lv_obj_set_style_bg_color(speedRoller, color(kColorPanelFocused), LV_PART_SELECTED);
        lv_obj_set_style_text_color(speedRoller, color(kColorText), 0);
        lv_obj_set_style_border_width(speedRoller, 0, 0);

        auto* playRow = createDetailRow(122, 50);
        auto* playTitle = createLabel(playRow, "播放/暂停", &lv_font_ui_cn_16);
        lv_obj_align(playTitle, LV_ALIGN_LEFT_MID, 15, 0);
        playLabel = createLabel(playRow, "暂停", &lv_font_ui_cn_16, kColorAccent);
        lv_obj_align(playLabel, LV_ALIGN_RIGHT_MID, -15, 0);

        orbitTrack = lv_obj_create(detailPage);
        lv_obj_set_size(orbitTrack, 144, 22);
        lv_obj_set_pos(orbitTrack, 48, 183);
        lv_obj_set_style_bg_color(orbitTrack, color(kColorTrack), 0);
        lv_obj_set_style_bg_opa(orbitTrack, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(orbitTrack, 0, 0);
        lv_obj_set_style_radius(orbitTrack, 11, 0);
        lv_obj_set_style_pad_all(orbitTrack, 0, 0);
        lv_obj_remove_flag(orbitTrack, LV_OBJ_FLAG_SCROLLABLE);
        orbitDot = lv_obj_create(orbitTrack);
        lv_obj_set_size(orbitDot, 14, 14);
        lv_obj_set_pos(orbitDot, 4, 4);
        lv_obj_set_style_bg_color(orbitDot, color(kColorAccentWarm), 0);
        lv_obj_set_style_bg_opa(orbitDot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(orbitDot, 0, 0);
        lv_obj_set_style_radius(orbitDot, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(orbitDot, LV_OBJ_FLAG_SCROLLABLE);
    }

    void buildButtonPage()
    {
        createDetailHeader("按键状态");
        for (std::size_t index = 0; index < buttonDots.size(); ++index) {
            auto* row = lv_obj_create(detailPage);
            lv_obj_set_size(row, 220, 47);
            lv_obj_set_pos(row, 10, 42 + static_cast<std::int32_t>(index) * 53);
            stylePanel(row);

            buttonDots[index] = lv_obj_create(row);
            lv_obj_set_size(buttonDots[index], 10, 10);
            lv_obj_set_pos(buttonDots[index], 12, 10);
            lv_obj_set_style_bg_color(buttonDots[index], color(kColorMuted), 0);
            lv_obj_set_style_bg_opa(buttonDots[index], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(buttonDots[index], 0, 0);
            lv_obj_set_style_radius(buttonDots[index], LV_RADIUS_CIRCLE, 0);
            lv_obj_remove_flag(buttonDots[index], LV_OBJ_FLAG_SCROLLABLE);

            auto* name = createLabel(row, kButtonNames[index], &lv_font_ui_cn_16);
            lv_obj_set_pos(name, 30, 4);
            buttonStateLabels[index] = createLabel(row, "释放", &lv_font_ui_cn_16, kColorMuted);
            lv_obj_align(buttonStateLabels[index], LV_ALIGN_TOP_RIGHT, -11, 4);
            buttonDetailLabels[index] = createLabel(row, "次数 0 / 0ms", &lv_font_ui_cn_16, kColorMuted);
            lv_obj_set_pos(buttonDetailLabels[index], 30, 24);
            lv_obj_set_width(buttonDetailLabels[index], 105);
            lv_label_set_long_mode(buttonDetailLabels[index], LV_LABEL_LONG_DOT);
            buttonEventLabels[index] = createLabel(row, "-", &lv_font_ui_cn_16, kColorMuted);
            lv_obj_set_width(buttonEventLabels[index], 82);
            lv_label_set_long_mode(buttonEventLabels[index], LV_LABEL_LONG_DOT);
            lv_obj_align(buttonEventLabels[index], LV_ALIGN_BOTTOM_RIGHT, -10, -2);
        }
        auto* hint = createLabel(detailPage, "最近事件 / 按压时长", &lv_font_ui_cn_16, kColorMuted);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -7);
        updateButtonPage();
    }

    void buildDetail(MenuPage page)
    {
        lv_obj_clean(detailPage);
        detailFocusTargets.clear();
        arc = nullptr;
        arcValue = nullptr;
        switches.fill(nullptr);
        speedRoller = nullptr;
        playLabel = nullptr;
        orbitTrack = nullptr;
        orbitDot = nullptr;
        buttonDots.fill(nullptr);
        buttonStateLabels.fill(nullptr);
        buttonDetailLabels.fill(nullptr);
        buttonEventLabels.fill(nullptr);

        switch (page) {
        case MenuPage::Arc: buildArcPage(); break;
        case MenuPage::Switches: buildSwitchPage(); break;
        case MenuPage::Animation: buildAnimationPage(); break;
        case MenuPage::Buttons: buildButtonPage(); break;
        case MenuPage::Home: break;
        }
    }

    void rebuildGroup()
    {
        if (group == nullptr) {
            return;
        }
        lv_group_remove_all_objs(group);
        const auto& state = model.snapshot();
        if (state.page == MenuPage::Home) {
            for (auto* card : cards) {
                lv_group_add_obj(group, card);
            }
            lv_group_focus_obj(cards[state.homeSelection]);
        }
        else {
            for (auto* target : detailFocusTargets) {
                lv_group_add_obj(group, target);
            }
            if (!detailFocusTargets.empty()) {
                const auto focus = std::min<std::size_t>(state.detailFocus, detailFocusTargets.size() - 1);
                lv_group_focus_obj(detailFocusTargets[focus]);
            }
        }
        lv_group_set_editing(group, state.mode == MenuMode::Edit);
    }

    lv_obj_t* focusedVisual() const
    {
        const auto& state = model.snapshot();
        if (state.page == MenuPage::Home) {
            return cards[state.homeSelection];
        }
        if (!detailFocusTargets.empty()) {
            const auto focus = std::min<std::size_t>(state.detailFocus, detailFocusTargets.size() - 1);
            return detailFocusTargets[focus];
        }
        return nullptr;
    }

    void updateDetailFocus()
    {
        const auto& state = model.snapshot();
        for (std::size_t index = 0; index < detailFocusTargets.size(); ++index) {
            const bool selected = index == state.detailFocus;
            auto* target = detailFocusTargets[index];
            lv_obj_set_style_border_width(target, selected ? 2 : 1, 0);
            lv_obj_set_style_border_color(
                target,
                color(selected
                          ? (state.switches[1] ? kColorAccent : kColorMuted)
                          : kColorTrack),
                0);
            lv_obj_set_style_bg_color(
                target,
                color(selected && state.switches[1] ? kColorPanelFocused : kColorPanel),
                0);
            animateValue(target,
                         setObjectX,
                         lv_obj_get_x(target),
                         selected ? 10 : 14,
                         MotionTiming::FocusMs);
        }
        if (detailMode != nullptr) {
            lv_label_set_text(detailMode, state.mode == MenuMode::Edit ? "编辑" : "浏览");
            lv_obj_set_style_text_color(
                detailMode, color(state.mode == MenuMode::Edit ? kColorAccentWarm : kColorAccent), 0);
        }
    }

    void updateArc()
    {
        if (arc == nullptr) {
            return;
        }
        const auto value = model.snapshot().arcValue;
        animateValue(arc,
                     setArcValue,
                     lv_arc_get_value(arc),
                     value,
                     MotionTiming::ValueMs);
        char text[8]{};
        std::snprintf(text, sizeof(text), "%u", static_cast<unsigned>(value));
        lv_label_set_text(arcValue, text);
        animateValue(arcValue, setObjectTranslateY, 3, 0, MotionTiming::ValueMs);
    }

    void updateSwitches()
    {
        const auto& state = model.snapshot();
        for (std::size_t index = 0; index < switches.size(); ++index) {
            if (switches[index] == nullptr) {
                continue;
            }
            const bool checked = lv_obj_has_state(switches[index], LV_STATE_CHECKED);
            if (checked == state.switches[index]) {
                continue;
            }
            if (state.switches[index]) {
                lv_obj_add_state(switches[index], LV_STATE_CHECKED);
            }
            else {
                lv_obj_remove_state(switches[index], LV_STATE_CHECKED);
            }
            lv_obj_send_event(switches[index], LV_EVENT_VALUE_CHANGED, nullptr);
        }
    }

    void updateAnimationPage()
    {
        const auto& state = model.snapshot();
        if (speedRoller != nullptr) {
            lv_roller_set_selected(speedRoller, state.animationSpeed, LV_ANIM_ON);
        }
        if (playLabel != nullptr) {
            lv_label_set_text(playLabel, state.animationPlaying ? "暂停" : "播放");
            lv_obj_set_style_text_color(
                playLabel, color(state.animationPlaying ? kColorAccent : kColorAccentWarm), 0);
        }
    }

    void updateButtonPage()
    {
        if (buttonDots[0] == nullptr) {
            return;
        }

        std::array<Telemetry, 3> snapshot;
        {
            std::lock_guard lock(telemetryMutex);
            snapshot = telemetry;
            telemetryDirty = false;
        }

        for (std::size_t index = 0; index < snapshot.size(); ++index) {
            const auto& item = snapshot[index];
            lv_obj_set_style_bg_color(
                buttonDots[index], color(item.pressed ? kColorAccentWarm : kColorMuted), 0);
            lv_label_set_text(buttonStateLabels[index], item.pressed ? "按下" : "释放");
            lv_obj_set_style_text_color(
                buttonStateLabels[index], color(item.pressed ? kColorAccentWarm : kColorMuted), 0);

            char details[48]{};
            std::snprintf(details,
                          sizeof(details),
                          "次数 %u / %ums",
                          static_cast<unsigned>(item.pressCount),
                          static_cast<unsigned>(item.heldMs));
            lv_label_set_text(buttonDetailLabels[index], details);
            lv_label_set_text(buttonEventLabels[index], item.lastEvent.c_str());
        }
    }

    void renderSamePage(const MenuSnapshot& before, bool animated)
    {
        const auto& state = model.snapshot();
        if (state.page == MenuPage::Home) {
            layoutHome(animated);
            if (state.boundaryFeedback != before.boundaryFeedback) {
                auto* card = cards[state.homeSelection];
                lv_anim_delete(card, setObjectX);
                lv_anim_t feedback;
                lv_anim_init(&feedback);
                lv_anim_set_var(&feedback, card);
                lv_anim_set_exec_cb(&feedback, setObjectX);
                lv_anim_set_values(&feedback, lv_obj_get_x(card), lv_obj_get_x(card) + 5);
                lv_anim_set_duration(&feedback, MotionTiming::PressMs);
                lv_anim_set_playback_duration(&feedback, MotionTiming::ReleaseMs);
                lv_anim_set_path_cb(&feedback, lv_anim_path_overshoot);
                lv_anim_start(&feedback);
            }
        }
        else {
            updateDetailFocus();
            switch (state.page) {
            case MenuPage::Arc: updateArc(); break;
            case MenuPage::Switches: updateSwitches(); break;
            case MenuPage::Animation: updateAnimationPage(); break;
            case MenuPage::Buttons: updateButtonPage(); break;
            case MenuPage::Home: break;
            }
        }
        rebuildGroup();
    }

    void transitionToDetail(MenuPage page)
    {
        buildDetail(page);
        updateDetailFocus();
        if (page == MenuPage::Switches) {
            updateSwitches();
        }
        else if (page == MenuPage::Animation) {
            updateAnimationPage();
        }

        lv_obj_move_foreground(detailPage);
        animateValue(homePage, setObjectX, lv_obj_get_x(homePage), -kScreenWidth, MotionTiming::PageMs);
        animateValue(detailPage, setObjectX, lv_obj_get_x(detailPage), 0, MotionTiming::PageMs);
        rebuildGroup();
    }

    void transitionToHome()
    {
        lv_obj_move_foreground(homePage);
        layoutHome(false);
        animateValue(detailPage, setObjectX, lv_obj_get_x(detailPage), kScreenWidth, MotionTiming::PageMs);
        animateValue(homePage, setObjectX, lv_obj_get_x(homePage), 0, MotionTiming::PageMs);
        rebuildGroup();
    }

    void pressFocused(bool pressed)
    {
        auto* target = pressed ? focusedVisual() : pressedTarget;
        if (target == nullptr) {
            return;
        }
        if (pressed) {
            if (pressedTarget != nullptr && pressedTarget != target
                && lv_obj_is_valid(pressedTarget)) {
                if (largeObjectLayersEnabled) {
                    setObjectScale(pressedTarget, kNormalScale);
                }
                else {
                    setObjectX(pressedTarget, pressedBaseX);
                }
            }
            pressedTarget = target;
            pressedBaseX = lv_obj_get_x(target);
            lv_obj_set_style_border_color(target, color(kColorAccentWarm), 0);
            if (largeObjectLayersEnabled) {
                animateValue(target,
                             setObjectScale,
                             lv_obj_get_style_transform_scale_x_safe(target, LV_PART_MAIN),
                             kPressedScale,
                             MotionTiming::PressMs);
            }
            else {
                animateValue(target,
                             setObjectX,
                             pressedBaseX,
                             pressedBaseX + 2,
                             MotionTiming::PressMs);
            }
        }
        else {
            if (lv_obj_is_valid(target)) {
                lv_obj_set_style_border_color(
                    target,
                    color(model.snapshot().switches[1] ? kColorAccent : kColorMuted),
                    0);
                if (largeObjectLayersEnabled) {
                    animateValue(target,
                                 setObjectScale,
                                 lv_obj_get_style_transform_scale_x_safe(target, LV_PART_MAIN),
                                 kNormalScale,
                                 MotionTiming::ReleaseMs,
                                 lv_anim_path_overshoot);
                }
                else {
                    animateValue(target,
                                 setObjectX,
                                 lv_obj_get_x(target),
                                 pressedBaseX,
                                 MotionTiming::ReleaseMs,
                                 lv_anim_path_overshoot);
                }
            }
            pressedTarget = nullptr;
        }
    }

    void handleAction(input::InputAction action)
    {
        if (!created) {
            return;
        }

        if (action == input::InputAction::ConfirmPressed) {
            pressFocused(true);
            return;
        }
        if (action == input::InputAction::ConfirmReleased) {
            pressFocused(false);
            return;
        }

        if (action == input::InputAction::Previous) {
            sendEncoder(-1);
        }
        else if (action == input::InputAction::Next) {
            sendEncoder(1);
        }
        else if (action == input::InputAction::Activate) {
            sendEnterClick();
        }

        const auto before = model.snapshot();
        if (!model.handleAction(action)) {
            return;
        }
        const auto after = model.snapshot();
        if (before.page == MenuPage::Home && after.page != MenuPage::Home) {
            transitionToDetail(after.page);
        }
        else if (before.page != MenuPage::Home && after.page == MenuPage::Home) {
            transitionToHome();
        }
        else {
            renderSamePage(before, true);
        }
        rendered = after;
    }

    void tick(std::uint32_t deltaMs)
    {
        if (!created) {
            return;
        }
        elapsedMs += deltaMs;
        animationAccumulatorMs += deltaMs;
        if (animationAccumulatorMs < policy.refreshPeriodMs) {
            return;
        }
        animationAccumulatorMs %= policy.refreshPeriodMs;

        if (model.snapshot().page == MenuPage::Buttons) {
            bool dirty{};
            {
                std::lock_guard lock(telemetryMutex);
                dirty = telemetryDirty;
            }
            if (dirty) {
                updateButtonPage();
            }
        }

        if (orbitDot == nullptr || model.snapshot().page != MenuPage::Animation) {
            return;
        }

        const auto& state = model.snapshot();
        static constexpr std::array<std::uint32_t, 3> traversalMs{2200, 1450, 850};
        const auto duration = traversalMs[state.animationSpeed];
        if (!state.animationPlaying) {
            if (policy.animateSmallLayers) {
                lv_obj_set_style_transform_scale(orbitDot, kNormalScale, 0);
                lv_obj_set_style_opa(orbitDot, LV_OPA_COVER, 0);
            }
            return;
        }

        const auto phase = elapsedMs % duration;
        const auto travel = 122U;
        const auto x = phase < duration / 2
            ? 4U + (phase * 2U * travel) / duration
            : 4U + ((duration - phase) * 2U * travel) / duration;
        lv_obj_set_x(orbitDot, static_cast<std::int32_t>(x));

        if (state.switches[0] && policy.animateSmallLayers) {
            const auto breathe = elapsedMs % MotionTiming::BreathingPeriodMs;
            const auto half = MotionTiming::BreathingPeriodMs / 2;
            const auto triangle = breathe < half ? breathe : MotionTiming::BreathingPeriodMs - breathe;
            const auto scale = 244 + static_cast<std::int32_t>((triangle * 24U) / half);
            lv_obj_set_style_transform_scale(orbitDot, scale, 0);
            lv_obj_set_style_opa(
                orbitDot,
                static_cast<lv_opa_t>(190U + (triangle * 65U) / half),
                0);
        }
        else if (policy.animateSmallLayers) {
            lv_obj_set_style_transform_scale(orbitDot, kNormalScale, 0);
            lv_obj_set_style_opa(orbitDot, LV_OPA_COVER, 0);
        }
    }

    [[nodiscard]] bool usesLargeObjectLayers() const noexcept
    {
        return largeObjectLayersEnabled;
    }

    void onButtonTelemetry(input::PhysicalButton button,
                           bool pressed,
                           std::uint32_t pressCount,
                           std::uint32_t heldMs,
                           std::string_view lastEvent)
    {
        std::lock_guard lock(telemetryMutex);
        auto& item = telemetry[buttonIndex(button)];
        item.pressed = pressed;
        item.pressCount = pressCount;
        item.heldMs = heldMs;
        if (!lastEvent.empty()) {
            item.lastEvent.assign(lastEvent);
        }
        telemetryDirty = true;
    }
};

MenuApplication::MenuApplication()
    : impl_(std::make_unique<Impl>())
{
}

MenuApplication::~MenuApplication() = default;

bool MenuApplication::create(lv_obj_t* root, const runtime::RenderPolicy& policy)
{
    return impl_->create(root, policy);
}

bool MenuApplication::usesLargeObjectLayers() const noexcept
{
    return impl_->usesLargeObjectLayers();
}

void MenuApplication::handleAction(input::InputAction action)
{
    impl_->handleAction(action);
}

void MenuApplication::tick(std::uint32_t elapsedMs)
{
    impl_->tick(elapsedMs);
}

void MenuApplication::onButtonTelemetry(input::PhysicalButton button,
                                        bool pressed,
                                        std::uint32_t pressCount,
                                        std::uint32_t heldMs,
                                        std::string_view lastEvent)
{
    impl_->onButtonTelemetry(button, pressed, pressCount, heldMs, lastEvent);
}

} // namespace app
